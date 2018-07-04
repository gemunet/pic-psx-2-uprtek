/****************************************************************************/
/* Programa que permite controlar el protocolo UPRTEK usando un joystick    */
/* psx one                                                                  */
/*                                                                          */
/* Author: gemu                                                             */
/* Created on 9 de mayo de 2013, 04:52 PM                                   */
/****************************************************************************/

#include <xc.h>

__CONFIG(CP_OFF & WDTE_OFF & FOSC_XT & PWRTE_ON);

/****************************************************************************/
/* CONFIGURACIONES                                                          */
/****************************************************************************/

/****************************************************************************/
/* Define datos del microcontrolador                                        */
/****************************************************************************/
#define _XTAL_FREQ      4000000


/****************************************************************************/
/* Define los pines que se usaran para controlar el Joystick PSX            */
/****************************************************************************/
#define PSX_PIN_DATA    PORTBbits.RB3
#define PSX_PIN_CMD     PORTAbits.RA1
#define PSX_PIN_ATT     PORTAbits.RA2
#define PSX_PIN_CLK     PORTAbits.RA0
#define PSX_DELAY_US    10


/****************************************************************************/
/* Definicion de pines del IRDA                                             */
/****************************************************************************/
#define IRDA_PIN_TX     PORTBbits.RB4


/****************************************************************************/
/* Configuracion de inicializacion del microcontrolador                     */
/****************************************************************************/
static void config_init() {
    // configura los pines de entrada/salida
    TRISBbits.TRISB3 = 1;
    OPTION_REGbits.nRBPU = 0; // activamos pull up del puerto B
    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 0;
    TRISAbits.TRISA2 = 0;

    // irda
    TRISBbits.TRISB4 = 0;
    TRISBbits.TRISB5 = 0;
    TRISBbits.TRISB6 = 0;
};


/****************************************************************************/
/* DEFINICIONES                                                             */
/****************************************************************************/

/****************************************************************************/
/* Estructura de definicion de los botones (2 bytes)                        */
/****************************************************************************/
typedef union {
    unsigned short data;
    struct {
        unsigned Right  :1;    // 0x0001
        unsigned Down   :1;    // 0x0002
        unsigned Left   :1;    // 0x0004
        unsigned Up     :1;    // 0x0008
        unsigned Start  :1;    // 0x0010
        unsigned None1  :1;    // 0x0020
        unsigned None2  :1;    // 0x0040
        unsigned Select :1;    // 0x0080
        unsigned Sqr    :1;    // 0x0100
        unsigned X      :1;    // 0x0200
        unsigned O      :1;    // 0x0400
        unsigned Tri    :1;    // 0x0800
        unsigned R1     :1;    // 0x1000
        unsigned L1     :1;    // 0x2000
        unsigned R2     :1;    // 0x4000
        unsigned L2     :1;    // 0x8000
    };
} PSXButtons;

/****************************************************************************/
/* Tiempos de bits del protocolo  UPRTEK                                    */
/****************************************************************************/
#define UPRTEK_HDR_MARK                 34
#define UPRTEK_HDR_SPACE                22
#define UPRTEK_ZERO_MARK                14
#define UPRTEK_ZERO_SPACE               18
#define UPRTEK_ONE_MARK                 30
#define UPRTEK_ONE_SPACE                34


/****************************************************************************/
/* Estructura de definicion de los comandos                                 */
/****************************************************************************/
struct TControl {
    unsigned char canal;
    unsigned char luz;
    unsigned char throtle;
    unsigned char giro;
    unsigned char derecha;
    unsigned char inclinacion;
    unsigned char adelante;
} control = {1, 0, 0, 0x3F, 0, 0x1F, 0};

unsigned char calibration_giro     = 0x3F;
unsigned char calibration_derecha  = 0;
unsigned long cmd = 0;

unsigned char F;
unsigned long uldata;
unsigned char cDataOut;
unsigned char cDataIn;

/****************************************************************************/
/* Realiza el intercambio de datos con el joystick                          */
/* Esta funcion escribe un byte de CMD y lee un byte de DATA sincronizando  */
/* y manejando el CLK durante todo el proceso.                              */
/* Retorno:                                                                 */
/*  unsigned char (1 byte) que contiene el DATA recibido por el joystick    */
/****************************************************************************/
unsigned char shift(unsigned char dataOut) {
    cDataOut = dataOut;
#asm
; clock de 50khz
    movlw   0x08
    clrf    _cDataIn

_bit
    ; flanco de bajada
    bcf     PORTA, 0          ; flanco de bajada del clock

    ; escribe un bit
    btfss   _cDataOut, 0
    bcf     PORTA, 1
    btfsc   _cDataOut, 0
    bsf     PORTA, 1
    bcf     STATUS, 0
    rrf     _cDataOut

    ; lee un bit
    bcf     STATUS, 0
    rlf     _cDataIn
    btfss   PORTB, 3
    bcf     _cDataIn, 0
    btfsc   PORTB, 3
    bsf     _cDataIn, 0
    goto    $+1

    ; flanco de subida
    bsf     PORTA, 0          ; flanco de subida del clock

    goto    $+1
    goto    $+1
    goto    $+1
    goto    $+1

    addlw   0xFF
    btfss   STATUS, 2
    goto    _bit
#endasm

    return cDataIn;
}

/****************************************************************************/
/* Inicializa datos para preparar la lectura del joystick                   */
/****************************************************************************/
void psx_init() {
    //PSX_PIN_DATA = 1;
    PSX_PIN_CMD = 1;
    PSX_PIN_ATT = 1;
    PSX_PIN_CLK = 1;
}

/****************************************************************************/
/* Lee un dato desde el joystick                                            */
/* Retorno:                                                                 */
/*  unsigned short (2 bytes) que contiene los bits activos de los botones   */
/****************************************************************************/
PSXButtons psx_read() {
    PSXButtons buttons;
    buttons.data = 0;
    PSX_PIN_ATT = 0;

    // header
    shift(0x01);
    shift(0x42);
    shift(0xFF);

    // data
    unsigned char data1 = ~shift(0xFF);
    unsigned char data2 = ~shift(0xFF);

    PSX_PIN_ATT = 1;
    buttons.data = (data2 << 8) | data1;

    return buttons;
}


/****************************************************************************/
/* Genera el checksum y lo inserta en Control                               */
/****************************************************************************/
unsigned long uprtek_build_command(struct TControl control) {
    unsigned long code = 0;

    // forma el comando con los datos de la estructura
    code |= control.throtle;
    code <<= 2;
    if(control.derecha)
        code |= 0x01;
    code <<= 6;
    code |= control.giro;
    code <<= 2;
    if(control.luz)
        code |= 0x01;
    code <<= 1;
    if(control.adelante)
        code |= 0x01;
    code <<= 5;
    code |= control.inclinacion;
    
    switch(control.canal) {
        case 1:
            code |= 0x80;
            break;
        case 2:
            code = 0x00;
            break;
        case 3:
            code |= 0x8080;
            break;
    }

    // genera el checksum
    unsigned int checksum = 0;
    for(int i = 5; i >= 0; i--) {
      checksum += (code >> (4*i)) & 0x0F;
    }
    checksum = (0x10 - checksum) & 0x0F;
    code <<= 4;
    code |= checksum;

    return code;
}

/****************************************************************************/
/* Envia un dato por el irda                                                */
/****************************************************************************/
void uprtek_send_command(unsigned long data) {
    uldata = data;
    
#asm
; envia el comando uprtek
uprtek_send_command
    ; header
    movlw   UPRTEK_HDR_MARK
    call    irdaMark
    movlw   UPRTEK_HDR_SPACE
    call    irdaSpace
    movlw   UPRTEK_HDR_MARK
    call    irdaMark
    movlw   UPRTEK_HDR_SPACE
    call    irdaSpace

    ; data (24 bits)
    btfss   _uldata+3, 3
    call    uprtekZero
    btfsc   _uldata+3, 3
    call    uprtekOne
    btfss   _uldata+3, 2
    call    uprtekZero
    btfsc   _uldata+3, 2
    call    uprtekOne
    btfss   _uldata+3, 1
    call    uprtekZero
    btfsc   _uldata+3, 1
    call    uprtekOne
    btfss   _uldata+3, 0
    call    uprtekZero
    btfsc   _uldata+3, 0
    call    uprtekOne

    btfss   _uldata+2, 7
    call    uprtekZero
    btfsc   _uldata+2, 7
    call    uprtekOne
    btfss   _uldata+2, 6
    call    uprtekZero
    btfsc   _uldata+2, 6
    call    uprtekOne
    btfss   _uldata+2, 5
    call    uprtekZero
    btfsc   _uldata+2, 5
    call    uprtekOne
    btfss   _uldata+2, 4
    call    uprtekZero
    btfsc   _uldata+2, 4
    call    uprtekOne
    btfss   _uldata+2, 3
    call    uprtekZero
    btfsc   _uldata+2, 3
    call    uprtekOne
    btfss   _uldata+2, 2
    call   uprtekZero
    btfsc   _uldata+2, 2
    call    uprtekOne
    btfss   _uldata+2, 1
    call    uprtekZero
    btfsc   _uldata+2, 1
    call    uprtekOne
    btfss   _uldata+2, 0
    call    uprtekZero
    btfsc   _uldata+2, 0
    call    uprtekOne

    btfss   _uldata+1, 7
    call    uprtekZero
    btfsc   _uldata+1, 7
    call    uprtekOne
    btfss   _uldata+1, 6
    call    uprtekZero
    btfsc   _uldata+1, 6
    call    uprtekOne
    btfss   _uldata+1, 5
    call    uprtekZero
    btfsc   _uldata+1, 5
    call    uprtekOne
    btfss   _uldata+1, 4
    call    uprtekZero
    btfsc   _uldata+1, 4
    call    uprtekOne
    btfss   _uldata+1, 3
    call    uprtekZero
    btfsc   _uldata+1, 3
    call    uprtekOne
    btfss   _uldata+1, 2
    call   uprtekZero
    btfsc   _uldata+1, 2
    call    uprtekOne
    btfss   _uldata+1, 1
    call    uprtekZero
    btfsc   _uldata+1, 1
    call    uprtekOne
    btfss   _uldata+1, 0
    call    uprtekZero
    btfsc   _uldata+1, 0
    call    uprtekOne
    
    btfss   _uldata, 7
    call    uprtekZero
    btfsc   _uldata, 7
    call    uprtekOne
    btfss   _uldata, 6
    call    uprtekZero
    btfsc   _uldata, 6
    call    uprtekOne
    btfss   _uldata, 5
    call    uprtekZero
    btfsc   _uldata, 5
    call    uprtekOne
    btfss   _uldata, 4
    call    uprtekZero
    btfsc   _uldata, 4
    call    uprtekOne
    btfss   _uldata, 3
    call    uprtekZero
    btfsc   _uldata, 3
    call    uprtekOne
    btfss   _uldata, 2
    call   uprtekZero
    btfsc   _uldata, 2
    call    uprtekOne
    btfss   _uldata, 1
    call    uprtekZero
    btfsc   _uldata, 1
    call    uprtekOne
    btfss   _uldata, 0
    call    uprtekZero
    btfsc   _uldata, 0
    call    uprtekOne

    ; foot
    movlw   UPRTEK_ONE_MARK
    call    irdaMark
    movlw   UPRTEK_ONE_SPACE
    call    irdaSpace
    return

; marca un 0
uprtekZero
    movlw   UPRTEK_ZERO_MARK
    call    irdaMark
    movlw   UPRTEK_ZERO_SPACE
    call    irdaSpace
    return

; marca un 1
uprtekOne
    movlw   UPRTEK_ONE_MARK
    call    irdaMark
    movlw   UPRTEK_ONE_SPACE
    call    irdaSpace
    return

irdaMark                            ; genera una marca de W ciclos de 40khz
    movwf   _F
irdaMarkCycle
    bsf     PORTB, 4                ; irda 1
    bsf     PORTB, 5                ; irda 2
    bsf     PORTB, 6                ; irda 3
    movlw   0x02
    addlw   -1
    btfss   STATUS, 2
    goto    $-2
    nop
    bcf     PORTB, 4                ; irda 1
    bcf     PORTB, 5                ; irda 2
    bcf     PORTB, 6                ; irda 3
    movlw   0x01
    addlw   -1
    btfss   STATUS, 2
    goto    $-2
    goto    $+1
    nop
    decfsz  _F
    goto    irdaMarkCycle
    return

irdaSpace                           ; genera una espacio de W ciclos de 40khz
    movwf   _F
irdaSpaceCycle
    bcf     PORTB, 4                ; irda 1
    bcf     PORTB, 5                ; irda 2
    bcf     PORTB, 6                ; irda 3
    movlw   0x04
    addlw   -1
    btfss   STATUS, 2
    goto    $-2
    goto    $+1
    decfsz  _F
    goto    irdaSpaceCycle
    return
#endasm

}


/****************************************************************************/
/* Inicializacion                                                           */
/****************************************************************************/
void setup() {
    config_init();
    psx_init();
}

/****************************************************************************/
/* Bucle principal de ejecucion                                             */
/****************************************************************************/
void loop() {
    PSXButtons buttons = psx_read();

    // aqui se setean los valores no persistentes del comando
    // para que al liberar el boton este se restablezca
    //control.
    control.giro = calibration_giro;
    control.derecha = calibration_derecha;
    control.inclinacion = 0x1F;
    control.adelante = 0;

    /*
     * Procesa las acciones
     */
    if(buttons.Start) {    // idle
        control.luz = 0;
        control.throtle = 0;
    }

    if(buttons.Select) {    // luz
        control.luz = !control.luz;
    }
    
    if(buttons.Up) {    // avanzar
        control.adelante = 1;
        control.inclinacion = 0x1F;
    }

    if(buttons.Down) {  // retroceder
        control.adelante = 0;
        control.inclinacion = 0;
    }

    if(buttons.Left) {  // izquierda
        control.derecha = 0;
        control.giro = 0x00;
    }

    if(buttons.Right) { // derecha
        control.derecha = 1;
        control.giro = 0x3F;
    }

    if(buttons.L1) {    // preset izquierda
        if(calibration_derecha) {
            if(calibration_giro < 0x3F)
                calibration_giro++;
        } else {
            if(calibration_giro < 0x3F) {
                calibration_giro++;
            } else {
                calibration_derecha = 1;
                calibration_giro = 0x00;
            }
        }

        control.derecha = calibration_derecha;
        control.giro = calibration_giro;
    }

    if(buttons.R1) {    // preset derecha
        if(!calibration_derecha) {
            if(calibration_giro > 0x00)
                calibration_giro--;
        } else {
            if(calibration_giro > 0x00) {
                calibration_giro--;
            } else {
                calibration_derecha = 0;
                calibration_giro = 0x3F;
            }
        }

        control.derecha = calibration_derecha;
        control.giro = calibration_giro;
    }

    if(buttons.Tri) {   // ascender
        if(control.throtle < 0xFF)
            control.throtle += 3;
    }

    if(buttons.X) {    // descender
        if(control.throtle > 0x00)
            control.throtle -= 3;
    }

    // genera checksum
    cmd = uprtek_build_command(control);
    
    // envia el codigo
    for(int i=0; i<3; i++) {
        uprtek_send_command(cmd);
        __delay_ms(10);
    }
    
    // delay
    __delay_ms(40);
}

void main(void) {
    setup();
    while(1)
        loop();
}