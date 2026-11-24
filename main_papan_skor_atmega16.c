/*
 * Program papan skor digital 2 pasang x 2 digit menggunakan ATmega16.
 * Asumsi koneksi sesuai gambar Proteus:
 *   PC0..PC6 : segment A..G melalui resistor 220 ohm
 *   PD0..PD3 : pemilih digit Q1..Q4 melalui transistor PNP BC557
 *   PB0      : tombol tambah skor kiri
 *   PB1      : tombol kurang skor kiri
 *   PB2      : tombol tambah skor kanan
 *   PB3      : tombol kurang skor kanan
 *   PA7      : buzzer melalui transistor NPN BC547
 *
 * Asumsi tampilan: common-anode; segmen aktif LOW dan BC557 aktif LOW.
 * Clock proyek: 8 MHz (samakan dengan Clock Frequency di Proteus).
 */

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BUTTON_MASK 0x0F
#define DIGIT_MASK  0x0F
#define BUZZER_PIN  PA7

/* Pola seven-segment common-cathode, bit 0..6 = A..G.
 * Keluaran dibalik saat dikirim ke display common-anode.
 */
static const uint8_t segment_cc[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
};

static uint8_t skor_kiri  = 0;
static uint8_t skor_kanan = 0;

/* Tampilkan satu digit. Q1..Q4 aktif LOW karena diasumsikan BC557 PNP. */
static void tampilkan_digit(uint8_t nomor_digit, uint8_t angka)
{
    /* Matikan semua digit sebelum mengubah pola segmen (mencegah ghosting). */
    PORTD |= DIGIT_MASK;

    /* Common-anode: segmen menyala dengan logika LOW. */
    PORTC = (uint8_t)(~segment_cc[angka] & 0x7F);

    /* Aktifkan satu digit. */
    PORTD &= (uint8_t)~(1U << nomor_digit);
    _delay_ms(2);

    /* Matikan kembali digit tersebut. */
    PORTD |= DIGIT_MASK;
}

/* Refresh keempat digit secara bergantian. */
static void refresh_display(void)
{
    tampilkan_digit(0, skor_kiri / 10);   /* Q1: puluhan kiri */
    tampilkan_digit(1, skor_kiri % 10);   /* Q2: satuan kiri  */
    tampilkan_digit(2, skor_kanan / 10); /* Q3: puluhan kanan */
    tampilkan_digit(3, skor_kanan % 10); /* Q4: satuan kanan  */
}

static void proses_tombol(uint8_t tombol_baru_ditekan)
{
    if (tombol_baru_ditekan & (1U << PB0)) {
        if (skor_kiri < 99) skor_kiri++;
    }

    if (tombol_baru_ditekan & (1U << PB1)) {
        if (skor_kiri > 0) skor_kiri--;
    }

    if (tombol_baru_ditekan & (1U << PB2)) {
        if (skor_kanan < 99) skor_kanan++;
    }

    if (tombol_baru_ditekan & (1U << PB3)) {
        if (skor_kanan > 0) skor_kanan--;
    }
}

int main(void)
{
    uint8_t raw_sebelumnya;
    uint8_t raw_stabil;
    uint8_t hitung_stabil = 0;
    uint8_t tombol_baru;
    uint8_t beep_scan = 0;

    /* PC2..PC5 juga merupakan pin JTAG pada ATmega16.
     * Disable JTAG agar semua PC0..PC6 dapat dipakai sebagai output segmen.
     */
#ifdef JTD
    {
        uint8_t status_jtag_off = MCUCSR | (1U << JTD);
        MCUCSR = status_jtag_off;
        MCUCSR = status_jtag_off;
    }
#endif

    /* PC0..PC6 sebagai output untuk segmen A..G. */
    DDRC |= 0x7F;
    PORTC = 0x7F; /* semua segmen mati untuk common-anode */

    /* PD0..PD3 sebagai output untuk pemilih digit. */
    DDRD |= DIGIT_MASK;
    PORTD |= DIGIT_MASK; /* semua transistor PNP mati */

    /* PB0..PB3 sebagai input tombol dengan pull-up internal. */
    DDRB &= (uint8_t)~BUTTON_MASK;
    PORTB |= BUTTON_MASK;

    /* PA7 sebagai output buzzer melalui BC547 (aktif HIGH). */
    DDRA |= (1U << BUZZER_PIN);
    PORTA &= (uint8_t)~(1U << BUZZER_PIN);

    raw_sebelumnya = PINB & BUTTON_MASK;
    raw_stabil = raw_sebelumnya;

    while (1) {
        /* Satu putaran refresh memindai semua empat digit. */
        refresh_display();

        /* Lama bunyi buzzer kira-kira beep_scan x 8 ms. */
        if (beep_scan > 0) {
            beep_scan--;
            if (beep_scan == 0) {
                PORTA &= (uint8_t)~(1U << BUZZER_PIN);
            }
        }

        /* Debounce tombol: keadaan harus stabil selama beberapa pemindaian. */
        {
            uint8_t raw_sekarang = PINB & BUTTON_MASK;

            if (raw_sekarang == raw_sebelumnya) {
                if (hitung_stabil < 3) hitung_stabil++;

                if ((hitung_stabil == 3) && (raw_sekarang != raw_stabil)) {
                    /* Pull-up: tombol yang baru ditekan berubah dari 1 ke 0. */
                    tombol_baru = (uint8_t)(raw_stabil & (uint8_t)~raw_sekarang & BUTTON_MASK);
                    raw_stabil = raw_sekarang;

                    if (tombol_baru != 0) {
                        proses_tombol(tombol_baru);
                        PORTA |= (1U << BUZZER_PIN);
                        beep_scan = 18; /* sekitar 144 ms */
                    }
                }
            } else {
                raw_sebelumnya = raw_sekarang;
                hitung_stabil = 0;
            }
        }
    }
}
