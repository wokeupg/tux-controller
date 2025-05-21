/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

// declare, this removes warnings
// int tux_buttons(struct tty_struct* tty, unsigned long arg);
// char display_hex(char hex_val, char dec_point);
// int tux_init(struct tty_struct* tty);
// int tux_set_led(struct tty_struct* tty, unsigned long arg);
// char display_hex(char hex_val, char dec_point);

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
int ack;							// global var flag
unsigned long buttoons;
unsigned long led_backup;
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;
	/* a is where command is stored, and b and c is where data is stored*/
	/* depending on a - manipulate b and c accordingly to that #*/

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

	/* switch case */
	switch(a) {

	case MTCP_ERROR :
    	break; 
	
	case MTCP_ACK :
    	ack = 0;
    	break; 
  
	case MTCP_RESET :
		tux_init(tty);
		// //ldisc put - how computer interacts with tux
		// //computer receives packets from tux
		// //tux im done with this code
		tux_set_led(tty, led_backup);		// 8->3 CHECK
		break;

	case MTCP_BIOC_EVENT:
    	// button_packet[0] = b;
    	// button_packet[1] = c;
		// buttoons = 0xFF;
		buttoons = (0x0F & b)| ((0x01 & c)<<4) | ((0x04 & c)<<3) | ((0x02 & c)<<5) | ((0x08 & c)<<4);
    	return;

	default : 
   		break;
}

    /*printk("packet : %x %x %x\n", a, b, c); */
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
 /* tuxctl.ioctl
 *   DESCRIPTION: Uses switch case to call different ioctl functions to initialize and set LEDs
 *   INPUTS: struct tty_struct* tty, struct file* file, unsigned cmd, unsigned long arg
 *   OUTPUTS: none
 *   RETURN VALUE: -EINVAL, tux_init(tty), tux_buttons(tty, arg), tux_set_led(tty, arg)
 *   SIDE EFFECTS: calls the tux ioctl functions
 *                 
 */

int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT:
		// MTCP_LED_USR give me control from oops
		//printk("something here");
		return tux_init(tty);	

	case TUX_BUTTONS:
		return tux_buttons(tty, arg);

	case TUX_SET_LED:			

		return tux_set_led(tty, arg);	// LED settings
		
	case TUX_LED_ACK:
		return 0;
	case TUX_LED_REQUEST:
		return 0;
	case TUX_READ_LED:
		return 0;
	default:
	    return -EINVAL;
    }
}

// unsigned char button_packet[2];

int tux_init(struct tty_struct* tty){
	unsigned char buffer[2];
	if(ack){
		return -EINVAL;
	}
	ack = 1;						//set ack - 1, my flag variable
	buttoons = 0xFF;				
	buffer[0] = MTCP_BIOC_ON;		// sets 0th element of buffer to bioc
	buffer[1] = MTCP_LED_USR;
	tuxctl_ldisc_put(tty, buffer, 2);
	ack = 0;						// initially our flag is zero for interrupt from kernel, tux, and user. They send packets to each other
	return 0;
}

 /* tux_set_led
 *   DESCRIPTION: Takes bits from arg and checks which decimal and LED lights should be turned on. It calls the function hex_display to get the values to dispay on the tux
 *   INPUTS: struct tty_struct* tty, unsigned long arg
 *   OUTPUTS:  NONE
 *   RETURN VALUE:  return tuxctl_ldisc_put(tty, led_buffer, 6)
 *   SIDE EFFECTS: calls the tuxctl_ldisc_put and display_hex function
 *                 
 */
int tux_set_led(struct tty_struct* tty, unsigned long arg){
	int i;
	char my_hex[4];
	char led_buffer[6];								// buffer is of size 6
	char my_dp;										// current hex value and current decimal point
	char led; 
	char dec, bit_mask_led, bit_mask_hex;

	if(ack){												//flag: let PC know we have arg
		return 0;
	}else{
	ack= 1;
	
	led_backup = arg;
	led_buffer[0] = MTCP_LED_SET;

	//char hex = arg & 0xFFFF; 								//gets us values in low 16 bits from arg which tells us which hex value to display
	led = ((arg>>16) & 0x0F);								// low 4 bits of 3rd byte tells us which LED should be turned on, 0 - off, 1- on, iterate through 4 bits to check whether each light is on or off
	led_buffer[1] = 0xF;									// gets us high 16 bits to display LED
	dec = ((arg>>24)& 0x0F);

	bit_mask_led = 0x01;					
	bit_mask_hex = 0x000F;

	
	my_hex[0] = (arg & 0x000F);								// grabs the first, second, third, and fourth hex bits to check
	my_hex[1] = (arg & 0x00F0)>>4;
	my_hex[2] = (arg & 0x0F00)>>8;
	my_hex[3] = (arg & 0xF000)>>12;
	
	for(i = 0; i<4; i++){									// iterate through each of the LEDS
	 if((led & bit_mask_led) == bit_mask_led){				// curr LED ON?
			my_dp = dec & bit_mask_led;						// dec is ON?
			led_buffer[i+2] = display_hex(my_hex[i], my_dp);
		}
		dec >>= 1;
		bit_mask_hex <<= 4;
		led>>=1;
	 }

	 return tuxctl_ldisc_put(tty, led_buffer, 6);					//tty or arg CHECK
	}
}	

/* tux_set_led
 *   DESCRIPTION: takes my_hex and my_dec from tux_set_led function and gets the the value/decimal point to be displayed and stores it into led_set
 *   INPUTS: char hex_val, char dec_point
 *   OUTPUTS:  NONE
 *   RETURN VALUE:  led_set
 *   SIDE EFFECTS: called by tux_set_led
 *                 
 */
char display_hex(char hex_val, char dec_point){
	char led_set;
	switch (hex_val){
		case 0x0:
			led_set = 0xE7;			//0
			break;
		case 0x1:
			led_set = 0x06;			//1
			break;
		case 0x2:
			led_set = 0xCB;			//2
			break;
		case 0x3:
			led_set = 0x8F;			//3
			break;
		case 0x4:
			led_set = 0x2E;			//4
			break;
		case 0x5:
			led_set = 0xAD;			//5
			break;
		case 0x6:
			led_set = 0xED;			//6
			break;
		case 0x7:
			led_set = 0x86;			//7
			break;
		case 0x8:
			led_set = 0xEF;			//8
			break;
		case 0x9:
			led_set = 0xAF;			//9
			break;
		case 0x10:
			led_set = 0xEE;			//A
			break;
		case 0x11:
			led_set = 0x6D;			//B
			break;
		case 0x12:
			led_set = 0xE1;			//C
			break;
		case 0x13:
			led_set = 0x4F;			//D
			break;
		case 0x14:
			led_set = 0xE9;			//E
			break;
		case 0x15:
			led_set = 0xE8;			//F
			break;

		default:
			return 0xAF;
	}
	if(dec_point == 1){
		led_set = (led_set | 0x10);
	}

	return led_set;
}

/* tux_buttons
 *   DESCRIPTION: Sends the argument and button pressed value info to user
 *   INPUTS: struct tty_struct* tty, unsigned long arg
 *   OUTPUTS:  NONE
 *   RETURN VALUE:  0
 *   SIDE EFFECTS: calls copy_to_user
 *                 
 */
int tux_buttons(struct tty_struct* tty, unsigned long arg){
	if (arg == 0){				//NULL
		return -EINVAL;
	}
	copy_to_user((unsigned long *)arg, &buttoons, 4);	//copies first four bytes of data from buf1 to the memory location pointed to by arg
	return 0;
}

// button packet 0 holds 1xxxCBA*, we want the last 4 bits
//shift over four and get top 4 bits

