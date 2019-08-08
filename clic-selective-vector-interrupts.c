/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>
#include <metal/cpu.h>
#include <metal/led.h>
#include <metal/button.h>
#include <metal/switch.h>

#define RTC_FREQ	32768

struct metal_cpu *cpu;
struct metal_interrupt *cpu_intr;
struct metal_interrupt *clic;
int tmr_id, swch1_irq;
struct metal_led *led0_red, *led0_green, *led0_blue;

void display_instruction (void) {
    printf("\n");
    printf("SIFIVE, INC.\n!!\n");
    printf("\n");
    printf("Coreplex IP Eval Kit 'clic-vector-interrupts' Example.\n\n");
    printf("IRQ 12 (CSIP) is enabled as extrenal interrupt sources.\n");
    printf("A 10s debounce timer is used between these interupts.\n");
    printf("\n");
}

void timer_isr (void) __attribute__((interrupt, aligned(64)));
void timer_isr (void) {

    printf("**** Lets trigger a clic software interrupt (after 10 seconds) ****\n");
    metal_interrupt_set(clic, swch1_irq);

    //metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
}

void switch1_isr(void) __attribute__((interrupt, aligned(64)));
void switch1_isr(void) {
    printf("Got CSIP interrupt on via IRQ %d!\n");
    metal_interrupt_clear(clic, swch1_irq);
    printf("Clear and re-arm timer another 10 seconds.\n");
    metal_led_toggle(led0_green);
    metal_led_toggle(led0_red);
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
}

int main (void)
{
    int rc;

    // Lets get start with getting LEDs and turn only RED ON
    led0_red = metal_led_get_rgb("LD0", "red");
    led0_green = metal_led_get_rgb("LD0", "green");
    led0_blue = metal_led_get_rgb("LD0", "blue");
    if ((led0_red == NULL) || (led0_green == NULL) || (led0_blue == NULL)) {
        printf("Abort. At least one of LEDs is null.\n");
        return 1;
    }
    metal_led_enable(led0_red);
    metal_led_enable(led0_green);
    metal_led_enable(led0_blue);
    metal_led_on(led0_red);
    metal_led_off(led0_green);
    metal_led_off(led0_blue);
 
    // Lets get the CPU and and its interrupt
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    if (cpu == NULL) {
        printf("Abort. CPU is null.\n");
        return 2;
    }
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    if (cpu_intr == NULL) {
        printf("Abort. CPU interrupt controller is null.\n");
        return 3;
    }
    metal_interrupt_init(cpu_intr);

    clic = metal_interrupt_get_controller(METAL_CLIC_CONTROLLER,
                                          metal_cpu_get_current_hartid());
    if (clic == NULL) {
        printf("Exit. This example need a clic interrupt controller.\n");
        return 0;
    }
    metal_interrupt_init(clic);
    metal_interrupt_set_vector_mode(clic, METAL_SELECTIVE_VECTOR_MODE);
    //metal_interrupt_privilege_mode(clic, METAL_INTR_PRIV_M_MODE);
    //metal_interrupt_set_threshold(clic, 4);

    tmr_id = metal_cpu_timer_get_interrupt_id(cpu);
    //metal_interrupt_vector_enable(clic, tmr_id);
    //metal_interrupt_set_priority(clic, tmr_id, 2);
    rc = metal_interrupt_register_vector_handler(clic, tmr_id, timer_isr, cpu);
    if (rc < 0) {
        printf("Failed. TIMER interrupt handler registration failed\n");
        return (rc * -1);
    }

    swch1_irq = 12;
    //metal_interrupt_vector_enable(clic, swch1_irq);
    //metal_interrupt_set_priority(clic, swch1_irq, 2);
    rc = metal_interrupt_register_vector_handler(clic, swch1_irq, switch1_isr, NULL);
    if (rc < 0) {
        printf("SW1 interrupt handler registration failed\n");
        return (rc * -1);
    }
    if (metal_interrupt_enable(clic, swch1_irq) == -1) {
        printf("SW1 interrupt enable failed\n");
        return 5;
    }
    // Set timeout of 10s, and enable timer interrupt
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
    metal_interrupt_enable(clic, tmr_id);

    display_instruction();

    // Lastly CPU interrupt
    if (metal_interrupt_enable(cpu_intr, 0) == -1) {
        printf("CPU interrupt enable failed\n");
        return 6;
    }

    while (1) {
        asm volatile ("wfi");
    }

    return 0;
}
