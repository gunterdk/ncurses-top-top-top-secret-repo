/*
 * TOP SECRET - text encryption program (ncurses)
 *
 * Two ciphers:
 *   1. Vigenere - every letter is shifted by the matching letter in the key
 *   2. XOR      - every character is XORed with the matching key character,
 *                 the result is shown as hex numbers
 *
 * Build (Linux / NixOS): gcc -o secret secret_simple.c -lncurses
 * Build (Windows MSYS):  gcc -o secret.exe secret_simple.c -lncurses -DNCURSES_STATIC
 * The terminal window must be at least 80x24.
 */

#include <curses.h>   /* ncurses library */
#include <stdio.h>    /* sprintf, sscanf */
#include <string.h>   /* strlen */
#include <ctype.h>    /* isalpha, isupper, islower, isxdigit */

#define MAXMSG   100            /* max length of the message the user types   */
#define MAXKEY   20             /* max length of the key                      */
#define MAXOUT   (MAXMSG * 2 + 1) /* hex output is twice as long as the input */
#define WIDTH    70             /* width of the main window                   */
#define HEIGHT   20             /* height of the main window                  */

/* Color pair numbers */
#define GREEN_ON_BLACK  1
#define RED_ON_BLACK    2
#define BLACK_ON_GREEN  3       /* used to highlight the selected menu item   */

#define VIGENERE 0
#define XOR      1

int cipher = VIGENERE;          /* the cipher that is currently selected      */
WINDOW *win;                    /* the main window                            */

/* ---------------------------------------------------------------------- */
/*  ENCRYPTION FUNCTIONS                                                  */
/* ---------------------------------------------------------------------- */

/* Vigenere. direction = 1 encrypts, direction = -1 decrypts.
 * Only letters are changed. Returns 0 if the key has no letters. */
int vigenere(const char *text, const char *key, char *out, int direction)
{
    int shifts[MAXKEY + 1];     /* the key converted to numbers 0-25          */
    int keylen = 0;
    int i, k = 0;

    /* go through the key and save the letters as numbers (A=0, B=1, ...) */
    for (i = 0; key[i] != '\0'; i++) {
        if (isalpha((unsigned char)key[i])) {
            shifts[keylen] = toupper((unsigned char)key[i]) - 'A';
            keylen++;
        }
    }
    if (keylen == 0) return 0;  /* no letters in the key -> error             */

    for (i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        char base;

        if (isupper((unsigned char)c))      base = 'A';
        else if (islower((unsigned char)c)) base = 'a';
        else {                  /* not a letter: copy it unchanged            */
            out[i] = c;
            continue;
        }
        /* shift the letter and wrap around the alphabet with % 26 */
        out[i] = (c - base + direction * shifts[k % keylen] + 26) % 26 + base;
        k++;                    /* move to the next key letter                */
    }
    out[i] = '\0';
    return 1;
}

/* XOR encrypt: every character is XORed with a key character and
 * written as two hex digits (for example "4F"). */
void xor_encrypt(const char *text, const char *key, char *out)
{
    int keylen = strlen(key);
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i] ^ key[i % keylen];
        sprintf(out + i * 2, "%02X", c);   /* write 2 hex digits */
    }
    out[i * 2] = '\0';
}

/* XOR decrypt: read the hex numbers back and XOR with the key again.
 * Returns 0 if the input is not valid hex. */
int xor_decrypt(const char *hex, const char *key, char *out)
{
    int keylen = strlen(key);
    int len = strlen(hex);
    int i;

    if (len % 2 != 0) return 0;            /* must be an even number of digits */
    for (i = 0; i < len; i++)
        if (!isxdigit((unsigned char)hex[i])) return 0;

    for (i = 0; i < len / 2; i++) {
        unsigned int number;
        sscanf(hex + i * 2, "%2x", &number);   /* read 2 hex digits */
        char c = number ^ key[i % keylen];
        out[i] = (c >= 32 && c < 127) ? c : '?'; /* hide unprintable characters */
    }
    out[len / 2] = '\0';
    return 1;
}

/* ---------------------------------------------------------------------- */
/*  SCREEN FUNCTIONS                                                      */
/* ---------------------------------------------------------------------- */

/* Clear the window, draw the border and the title. Also draws the top
 * and bottom line of the screen. */
void draw_screen(const char *title)
{
    erase();
    attron(COLOR_PAIR(BLACK_ON_GREEN));
    mvhline(0, 0, ' ', COLS);                        /* top bar    */
    mvprintw(0, 2, "*** TOP SECRET ***");
    mvhline(LINES - 1, 0, ' ', COLS);                /* bottom bar */
    mvprintw(LINES - 1, 2, "Cipher: %s", cipher == VIGENERE ? "VIGENERE" : "XOR");
    attroff(COLOR_PAIR(BLACK_ON_GREEN));
    wnoutrefresh(stdscr);

    werase(win);
    box(win, 0, 0);                                  /* border     */
    mvwprintw(win, 0, 3, " %s ", title);
    touchwin(win);
}

/* Show a menu and let the user choose with the arrow keys + Enter.
 * Returns the number of the chosen item (0, 1, 2, ...). */
int menu(const char *title, const char *items[], int count)
{
    int selected = 0;
    int i, key;

    while (1) {
        draw_screen(title);
        for (i = 0; i < count; i++) {
            if (i == selected) wattron(win, COLOR_PAIR(BLACK_ON_GREEN)); /* highlight */
            mvwprintw(win, 6 + i * 2, 24, " %-22s ", items[i]);
            if (i == selected) wattroff(win, COLOR_PAIR(BLACK_ON_GREEN));
        }
        mvwprintw(win, HEIGHT - 2, 3, "Arrow keys = move, Enter = select");
        wrefresh(win);

        key = wgetch(win);                           /* wait for a key */
        if (key == KEY_UP && selected > 0)           selected--;
        else if (key == KEY_DOWN && selected < count - 1) selected++;
        else if (key == '\n' || key == '\r' || key == KEY_ENTER) return selected;
    }
}

/* Read a line of text at position (y, x). If hidden is 1 the typed
 * characters are shown as stars (used for the key). */
void read_input(int y, int x, char *buffer, int maxlen, int hidden)
{
    int len = 0;
    int key;

    buffer[0] = '\0';
    curs_set(1);                                     /* show the cursor */
    while (1) {
        wmove(win, y, x + len);
        wrefresh(win);
        key = wgetch(win);

        if (key == '\n' || key == '\r' || key == KEY_ENTER) break;

        if ((key == KEY_BACKSPACE || key == 127 || key == '\b') && len > 0) {
            len--;                                   /* delete last character */
            buffer[len] = '\0';
            mvwaddch(win, y, x + len, ' ');
        }
        else if (key >= 32 && key < 127 && len < maxlen) {
            buffer[len] = key;                       /* add the character */
            len++;
            buffer[len] = '\0';
            mvwaddch(win, y, x + len - 1, hidden ? '*' : key);
        }
    }
    curs_set(0);                                     /* hide the cursor again */
}

/* Show a message in red and wait for a key. */
void show_error(const char *text)
{
    wattron(win, COLOR_PAIR(RED_ON_BLACK));
    mvwprintw(win, HEIGHT - 4, 3, "ERROR: %s", text);
    wattroff(win, COLOR_PAIR(RED_ON_BLACK));
    mvwprintw(win, HEIGHT - 2, 3, "Press any key...");
    wrefresh(win);
    wgetch(win);
}

/* The screen used for both encrypting (mode = 1) and decrypting (mode = 0) */
void crypt_screen(int mode)
{
    char input[MAXOUT + 1];     /* message or ciphertext from the user */
    char key[MAXKEY + 1];
    char result[MAXOUT + 1];
    int ok, i, row, len;

    draw_screen(mode ? "ENCRYPT" : "DECRYPT");

    mvwprintw(win, 2, 3, mode ? "Message:" : "Ciphertext:");
    read_input(3, 3, input, mode ? MAXMSG : MAXMSG * 2, 0);

    mvwprintw(win, 9, 3, "Key:");
    read_input(10, 3, key, MAXKEY, 1);

    if (input[0] == '\0' || key[0] == '\0') {
        show_error("Message and key cannot be empty");
        return;
    }

    /* run the selected cipher */
    if (cipher == VIGENERE) {
        ok = vigenere(input, key, result, mode ? 1 : -1);
    } else if (mode) {
        xor_encrypt(input, key, result);
        ok = 1;
    } else {
        ok = xor_decrypt(input, key, result);
    }
    if (!ok) {
        show_error(cipher == VIGENERE ? "Key must contain letters"
                                      : "Ciphertext must be hex numbers");
        return;
    }

    /* show the result, 60 characters per line */
    mvwprintw(win, 12, 3, mode ? "Encrypted:" : "Decrypted:");
    wattron(win, COLOR_PAIR(RED_ON_BLACK) | A_BOLD);
    len = strlen(result);
    for (i = 0, row = 13; i < len; i += 60, row++)
        mvwprintw(win, row, 3, "%.60s", result + i);
    wattroff(win, COLOR_PAIR(RED_ON_BLACK) | A_BOLD);

    mvwprintw(win, HEIGHT - 2, 3, "Press any key...");
    wrefresh(win);
    wgetch(win);
}

/* ---------------------------------------------------------------------- */
/*  MAIN                                                                  */
/* ---------------------------------------------------------------------- */

int main(void)
{
    const char *main_items[]   = { "Encrypt message", "Decrypt message",
                                   "Choose cipher", "Exit" };
    const char *cipher_items[] = { "Vigenere", "XOR (hex)" };
    int choice;

    initscr();                  /* start ncurses                              */
    cbreak();                   /* get keys without pressing Enter            */
    noecho();                   /* do not print typed keys automatically      */
    keypad(stdscr, TRUE);       /* enable arrow keys                          */
    curs_set(0);                /* hide the cursor                            */

    if (LINES < 24 || COLS < 80) {
        endwin();
        printf("Please make the terminal at least 80x24.\n");
        return 1;
    }

    start_color();              /* enable colors                              */
    init_pair(GREEN_ON_BLACK, COLOR_GREEN, COLOR_BLACK);
    init_pair(RED_ON_BLACK,   COLOR_RED,   COLOR_BLACK);
    init_pair(BLACK_ON_GREEN, COLOR_BLACK, COLOR_GREEN);
    bkgd(COLOR_PAIR(GREEN_ON_BLACK));

    win = newwin(HEIGHT, WIDTH, 2, (COLS - WIDTH) / 2);   /* centered window  */
    keypad(win, TRUE);
    wbkgd(win, COLOR_PAIR(GREEN_ON_BLACK));

    while (1) {
        choice = menu("MAIN MENU", main_items, 4);
        if (choice == 0)      crypt_screen(1);
        else if (choice == 1) crypt_screen(0);
        else if (choice == 2) cipher = menu("CHOOSE CIPHER", cipher_items, 2);
        else                  break;                      /* Exit */
    }

    endwin();                   /* restore the normal terminal                */
    return 0;
}
