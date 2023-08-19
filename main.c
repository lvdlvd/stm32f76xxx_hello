#include "cortex_cm7.h"
#include "stm32f7x7.h"

#include "clock.h"
#include "gpio2.h"
#include "nvic.h"

enum {
	LED0_PIN      = PB7, // 144 pin nucleo 
	USART2_TX_PIN = PA2,
};

/* clang-format off */
static struct gpio_config_t {
    enum GPIO_Pin  pins;
    enum GPIO_Conf mode;
} pin_cfgs[] = {
    {LED0_PIN, GPIO_OUTPUT},
//    {USART2_TX_PIN, GPIO_AF7_USART1|GPIO_HIGH},
    {0, 0}, // sentinel
};

enum { IRQ_PRIORITY_GROUPING = 5 }; // prio[7:6] : 4 groups,  prio[5:4] : 4 subgroups
struct {
    enum IRQn_Type irq;
    uint8_t        group, sub;
} irqprios[] = {
    {SysTick_IRQn, 0, 0},
    // {USART2_IRQn,  1, 0},
    // {USB_LP_IRQn,  2, 0},
    // {TIM3_IRQn,    3, 0},
    {None_IRQn, 0xff, 0xff},
};
/* clang-format on */

static inline void led0_off(void) { digitalLo(LED0_PIN); }
static inline void led0_on(void) { digitalHi(LED0_PIN); }
static inline void led0_toggle(void) { digitalToggle(LED0_PIN); }

extern uint32_t UNIQUE_DEVICE_ID[3]; // Section 48.1

#if 0
static struct Ringbuffer usart2tx;
void                     USART2_Handler(void) { usart_irq_handler(&USART2, &usart2tx); }
 size_t            u2puts(const char* buf, size_t len) { return usart_puts(&USART2, &usart2tx, buf, len); }
//static size_t 			 usb_puts(const char* buf, size_t len) { return usb_send(buf, len); }

void USB_LP_Handler(void) {
    uint64_t now = cycleCount();
    led0_toggle();
    cbprintf(u2puts, "%lld USB IRQ: %s\n", now / 72, usb_state_str(usb_state()));
    uint8_t buf[64];
    size_t  len = usb_recv(buf, sizeof buf);
    if (len > 0) {
        cbprintf(u2puts, "received %i: %*s\n", len, len, buf);
    }
}

void TIM3_Handler(void) {
    if ((TIM3.SR & TIM1_SR_UIF) == 0)
        return;
    TIM3.SR &= ~TIM1_SR_UIF;
    cbprintf(u2puts, "USB %s\n", usb_state_str(usb_state()));
    cbprintf(u2puts, "  fnr %u lsof %u D%s%s%s\n",  usb_fnr_get_fn(), usb_fnr_get_lsof(), 
    	((USB.FNR&USB_FNR_RXDP) ? "P" : "_"), 
    	((USB.FNR&USB_FNR_RXDM) ? "M":"_"), 
    	((USB.FNR&USB_FNR_LCK)  ? " LCK" : "")
    );
}
#endif

void main(void) __attribute__ ((noreturn));
void main(void) {
	uint8_t rf = (RCC.CSR >> 24) & 0xfc;
    (void)rf;
	RCC.CSR |= RCC_CSR_RMVF; // Set RMVF bit to clear the reset flags

    RCC.AHB1ENR  |= RCC_AHB1ENR_GPIOBEN;

    for (const struct gpio_config_t* p = pin_cfgs; p->pins; ++p) {
        gpioConfig(p->pins, p->mode);
    }



    for(;;) {
        led0_toggle();
        delay(500*1000);
    }

#if 0

    NVIC_SetPriorityGrouping(IRQ_PRIORITY_GROUPING);
    for (int i = 0; irqprios[i].irq != None_IRQn; i++) {
        NVIC_SetPriority(irqprios[i].irq, NVIC_EncodePriority(IRQ_PRIORITY_GROUPING, irqprios[i].group, irqprios[i].sub));
    }


	RCC.APB1ENR1 |= RCC_APB1ENR1_USART2EN | RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_USBEN;
	RCC.AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;

	for (const struct gpio_config_t* p = pin_cfgs; p->pins; ++p) {
		gpioConfig(p->pins, p->mode);
	}

	usart_init(&USART2, 921600);
	NVIC_EnableIRQ(USART2_IRQn);

	cbprintf(u2puts, "SWREV:%s\n", __REVISION__);
	cbprintf(u2puts, "CPUID:%08lx\n", SCB.CPUID);
	/// DEVID:20333631:5034500d:004f001d is aski encoded lot/wafer/x/y (see manual)
	cbprintf(u2puts, "DEVID:%08lx:%08lx:%08lx\n", UNIQUE_DEVICE_ID[2], UNIQUE_DEVICE_ID[1], UNIQUE_DEVICE_ID[0]);
	cbprintf(u2puts, "RESET:%02x%s%s%s%s%s%s\n", rf, rf & 0x80 ? " LPWR" : "", rf & 0x40 ? " WWDG" : "", rf & 0x20 ? " IWDG" : "",
	         rf & 0x10 ? " SFT" : "", rf & 0x08 ? " POR" : "", rf & 0x04 ? " PIN" : "");
	usart_wait(&USART2);

	// enable 1Hz TIM3
    TIM3.DIER |= TIM1_DIER_UIE;
    TIM3.PSC = 7200 - 1;  // 72MHz/7200   = 10KHz
    TIM3.ARR = 10000 - 1; // 10KHz/10000  = 1Hz
    TIM3.CR1 |= TIM1_CR1_CEN;
    NVIC_EnableIRQ(TIM3_IRQn);

	RCC_APB1ENR1 |= RCC_APB1ENR1_CRSEN;
	RCC.CRRCR |= RCC_CRRCR_HSI48ON;
	while((RCC.CRRCR & RCC_CRRCR_HSI48RDY) == 0)
		__NOP();
	crs_cfgr_set_syncsrc(&CRS, 2);  // USB SOF frames
	crs_cfgr_set_syncdiv(&CRS, 0);  // undivided
	crs_cfgr_set_felim(&CRS, 0x22);   // default
	crs_cfgr_set_reload(&CRS, (48000000/1000)-1);   // 48MHz/1KHz
	crs_cr_set_trim(&CRS, 0x40);
	CRS.CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
	rcc_ccipr_set_clk48sel(&RCC, 0); // 7.4.26, p.328:  0: HSI48, 2:PLLQ other reserved

    // USB Type-C and Power Delivery Dead Battery disable. After exiting reset, the USB Type-C “dead battery” behavior is enabled, 
    // which may have a pull-down effect on CC1 and CC2 pins. It is recommended to disable it in all cases, either to stop this 
    // pull-down or to hand over control to the UCPD1 (which should therefore be initialized before doing the disable).

    RCC.APB1ENR1 |= RCC_APB1ENR1_PWREN; // WAS THIS IT? TEST!
    PWR.CR3 |= PWR_CR3_UCPD1_DBDIS; 

    NVIC_EnableIRQ(USB_LP_IRQn);
    usb_init();

 	for (;;)
        __WFI(); // wait for interrupt to change the state of any of the subsystems
#endif

}