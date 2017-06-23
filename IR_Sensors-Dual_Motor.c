/* 10-30-13
 * This is an example of using the ADC to convert a single
 * analog input. The external input is to ADC10 channel A4.
 * This version triggers a conversion with regular WDT interrupts and
 * uses the ADC10 interrupt to copy the converted value to a variable in memory.
*/

#include "msp430g2553.h"

/** FOR MOTORS **/
// define direction register, output register, and select registers
#define TA_DIR P1DIR
#define TA_OUT P1OUT
#define TA_SEL P1SEL
#define BUTTON 0x08

// define the bit mask (within the port) corresponding to output TA1
#define TA_BIT 0x04

/** FOR MOTORS **/
// define direction register, output register, and select registers
#define TB_DIR P2DIR
#define TB_OUT P2OUT
#define TB_SEL P2SEL

// define the bit mask (within the port) corresponding to output TA1
#define TB_BIT 0x02

/** END FOR MOTORS **/

// Define bit masks for ADC pin and channel used as P1.4
#define ADC_INPUT_BIT_MASK 0x30
#define ADC_INCHR INCH_4
#define ADC_INCHL INCH_5

 /* declarations of functions defined later */
 void init_adc(void);
 void init_wdt(void);
 void init_timer(void);

// =======ADC Initialization and Interrupt Handler========

// Global variables that store the results (read from the debugger)
 volatile int latest_result;   // most recent result is stored in latest_result
 volatile int flag;
 volatile int right;
 volatile int left;
 volatile unsigned long conversion_count=0; //total number of conversions done
 volatile int adcDone;
 // Global state variables
 volatile unsigned char last_button; // the state of the button bit at the last interrupt
 volatile int motorOn;
 volatile int averageSensorValue;

/*
 * The ADC handler is invoked when a conversion is complete.
 * Its action here is to simply store the result in memory.
 */
 void interrupt adc_handler(){
	 if (flag == 0) {
		 right = ADC10MEM;
		 ADC10CTL0 &= ~ENC;
		 flag = 1;
		 return;
	 }
	 else if (flag == 1){
		 left = ADC10MEM;
		 ADC10CTL0 &= ~ENC;
		 flag = 0;
		 return;
	 }

	 latest_result=ADC10MEM;   // store the answer
	 ++conversion_count;       // increment the total conversion count
	 adcDone = 1;
 }
 ISR_VECTOR(adc_handler, ".int05")

// Initialization of the ADC
void init_adc(){
  ADC10CTL1= ADC_INCHR	//input channel 4
 			  +SHS_0 //use ADC10SC bit to trigger sampling
 			  +ADC10DIV_4 // ADC10 clock/5
 			  +ADC10SSEL_0 // Clock Source=ADC10OSC
 			  +CONSEQ_0; // single channel, single conversion
 			  ;
  ADC10AE0=ADC_INPUT_BIT_MASK; // enable A4 analog input
  ADC10CTL0= SREF_0	//reference voltages are Vss and Vcc
 	          +ADC10SHT_3 //64 ADC10 Clocks for sample and hold time (slowest)
 	          +ADC10ON	//turn on ADC10
 	          +ENC		//enable (but not yet start) conversions
 	          +ADC10IE  //enable interrupts
 	          ;
}


 // ===== Watchdog Timer Interrupt Handler =====
interrupt void WDT_interval_handler(){
		adcDone = 0;
		if (flag == 0){
			ADC10CTL0 &= ~ENC;				// Disable ADC
				ADC10CTL0 = ADC10SHT_3 + ADC10ON + ADC10IE;	// 16 clock ticks, ADC On, enable ADC interrupt
				ADC10CTL1 = ADC10SSEL_3 + ADC_INCHR;				// Set 'chan', SMCLK
				ADC10CTL0 |= ENC + ADC10SC;             	// Enable and start conversion
		}
		else if (flag == 1){
			ADC10CTL0 &= ~ENC;				// Disable ADC
				ADC10CTL0 = ADC10SHT_3 + ADC10ON + ADC10IE;	// 16 clock ticks, ADC On, enable ADC interrupt
				ADC10CTL1 = ADC10SSEL_3 + ADC_INCHL;				// Set 'chan', SMCLK
				ADC10CTL0 |= ENC + ADC10SC;             	// Enable and start conversion
		}
		//ADC10CTL0 |= ADC10SC;  // trigger a conversion

// ** CALIBRATION BUTTON ** //
		unsigned char b;
		b= (P1IN & BUTTON);  // read the BUTTON bit
		if (last_button && (b==0)){ // has the button bit gone from high to low
			// Button press action:
			//if (left > right) averageSensorValue = (left-right) / 2;
			//else if (right > left) averageSensorValue = (right-left) / 2;
			if (motorOn == 1){
				motorOn = 0;
				TA0CCR1 = 0;
				TA1CCR1 = 0;
			}
			else motorOn = 1;
		}
		last_button=b;    // remember button reading for next time.
// ** END CALIBRATION BUTTON ** //

		/** Logic for motor control **/
		/** follows light **/
		if (left > (right + 200) && motorOn == 1) {
			TA1CCR1 = 9999;
			TA0CCR1 = 3000;
		}
		else if (right > (left + 200) && motorOn == 1) {
			TA0CCR1 = 9999;
			TA1CCR1 = 3000;
		}
		else if ( motorOn == 1){
			TA0CCR1 = 9999;
			TA1CCR1 = 9999;
		}
		/** END LOGIC ABOVE **/
}
ISR_VECTOR(WDT_interval_handler, ".int10")

 void init_wdt(){
	// setup the watchdog timer as an interval timer
  	WDTCTL =(WDTPW +		// (bits 15-8) password
     	                   	// bit 7=0 => watchdog timer on
       	                 	// bit 6=0 => NMI on rising edge (not used here)
                        	// bit 5=0 => RST/NMI pin does a reset (not used here)
           	 WDTTMSEL +     // (bit 4) select interval timer mode
  		     WDTCNTCL  		// (bit 3) clear watchdog timer counter
  		                	// bit 2=0 => SMCLK is the source
  		                	// bits 1-0 = 00 => source/32K
 			 );
     IE1 |= WDTIE;			// enable the WDT interrupt (in the system interrupt register IE1)
 }

/*
 * The main program just initializes everything and leaves the action to
 * the interrupt handlers!
 */

void init_timer(){
	//*******SETUP THE TIMER (ONCE ONLY)
		TACTL = TACLR;					// reset clock
		TACTL = TASSEL_2+ID_3;			// clock source = SMCLK
										// clock divider=8
										// (clock still off)

		TACCTL0=0;						// Nothing on CCR0
		TACCTL1=OUTMOD_7;				// reset/set mode
		TACCR0 = 9999;					// period-1 in CCR0
		TACCR1 = 0;                  // duty cycle in CCR1
		TA_SEL|=TA_BIT;				// connect timer 0 output to pin 2
		TA_DIR|=TA_BIT;

		TACTL |= MC_1;					// timer on in up mode

	//*******END OF TIMER SETUP
	//*******SETUP THE TIMER (ONCE ONLY)

		TA1CTL = TACLR;					// reset clock
		TA1CTL = TASSEL_2+ID_3;			// clock source = SMCLK
										// clock divider=8
										// (clock still off)

		TA1CCTL0=0;						// Nothing on CCR0
		TA1CCTL1=OUTMOD_7;				// reset/set mode
		TA1CCR0 = 9999;					// period-1 in CCR0
		TA1CCR1 = 0;                  // duty cycle in CCR1
		TB_SEL|=TB_BIT;				// connect timer 1 output to pin 1
		TB_DIR|=TB_BIT;

		TA1CTL |= MC_1;					// timer on in up mode

	//*******END OF TIMER SETUP
}

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;
  	P1DIR &= ~BUTTON;						// make sure BUTTON bit is an input
    P1OUT |= BUTTON;
    P1REN |= BUTTON;						// Activate pullup resistors on Button Pin

    motorOn = 0;
  	left = -1;
  	right = -1;
  	flag = 0;
  	init_timer();
  	init_adc();
  	init_wdt();

	_bis_SR_register(GIE+LPM0_bits);
}
