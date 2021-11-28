#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "SDL2/SDL.h"
#include "chip8.h"

const char chip8_default_character_set[] = {
    0xf0, 0x90, 0x90, 0x90, 0xf0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xf0, 0x10, 0xf0, 0x80, 0xf0,
    0xf0, 0x10, 0xf0, 0x10, 0xf0,
    0x90, 0x90, 0xf0, 0x10, 0x10,
    0xf0, 0x80, 0xf0, 0x10, 0xf0,
    0xf0, 0x80, 0xf0, 0x90, 0xf0,
    0xf0, 0x10, 0x20, 0x40, 0x40,
    0xf0, 0x90, 0xf0, 0x90, 0xf0,
    0xf0, 0x90, 0xf0, 0x10, 0xf0,
    0xf0, 0x90, 0xf0, 0x90, 0x90,
    0xe0, 0x90, 0xe0, 0x90, 0xe0,
    0xf0, 0x80, 0x80, 0x80, 0xf0,
    0xe0, 0x90, 0x90, 0x90, 0xe0,
    0xf0, 0x80, 0xf0, 0x80, 0xf0,
    0xf0, 0x80, 0xf0, 0x80, 0x80,
};

void chip8_init(struct chip8 *chip8)
{
  memset(chip8, 0, sizeof(struct chip8));
  memcpy(&chip8->memory.memory, chip8_default_character_set, sizeof(chip8_default_character_set));
}

void chip8_load(struct chip8 *chip8, const char *buf, size_t size)
{
  assert(size + CHIP8_PROGRAM_LOAD_ADDRESS < CHIP8_MEMORY_SIZE);
  memcpy(&chip8->memory.memory[CHIP8_PROGRAM_LOAD_ADDRESS], buf, size);
  chip8->registers.PC = CHIP8_PROGRAM_LOAD_ADDRESS;
}

static void chip8_exec_extended_eight(struct chip8 *chip8, unsigned short opcode)
{
  unsigned char x = (opcode >> 8) & 0x000f;
  unsigned char y = (opcode >> 4) & 0x000f;
  unsigned char final_fourbits = opcode & 0x000f;
  unsigned short tmp = 0;

  switch (final_fourbits)
  {

  // LD Vx, Vy - 8xy0, set Vx = Vy
  case 0x00:
    chip8->registers.V[x] = chip8->registers.V[y];
    break;

  // OR Vx, Vy - 8xy1, set Vx = Vx | Vy
  case 0x01:
    chip8->registers.V[x] = chip8->registers.V[x] | chip8->registers.V[y];
    break;

  // AND Vx, Vy - 8xy2, set Vx = Vx & Vy
  case 0x02:
    chip8->registers.V[x] = chip8->registers.V[x] & chip8->registers.V[y];
    break;

  // XOR Vx, Vy - 8xy3, set Vx = Vx ^ Vy
  case 0x03:
    chip8->registers.V[x] = chip8->registers.V[x] ^ chip8->registers.V[y];
    break;

  // ADD Vx, Vy - 8xy4, set Vx = Vx + Vy, set Vf = carry
  case 0x04:
    tmp = chip8->registers.V[x] + chip8->registers.V[y];
    chip8->registers.V[0x0f] = false;
    if (tmp > 0xff)
    {
        chip8->registers.V[0x0f] = true;
    }

    chip8->registers.V[x] = tmp;
    break;

  // SUB Vx, Vy - 8xy5, set Vx = Vx - Vy, set Vf = NOT borrow
  case 0x05:
    chip8->registers.V[0x0f] = false;
    if (chip8->registers.V[x] > chip8->registers.V[y])
    {
        chip8->registers.V[0x0f] = true;
    }
    chip8->registers.V[x] = chip8->registers.V[x] - chip8->registers.V[y];
    break;

  // SHR Vx {, Vy } - 8xy6
  case 0x06:
    chip8->registers.V[0x0f] = chip8->registers.V[x] & 0x01;
    chip8->registers.V[x] = chip8->registers.V[x] / 2;
    break;

  // SUBN Vx, Vy - 8xy7, set Vx = Vy - Vx, set Vf = NOT borrow
  case 0x07:
    chip8->registers.V[0x0f] = chip8->registers.V[y] > chip8->registers.V[x];
    chip8->registers.V[x] = chip8->registers.V[y] - chip8->registers.V[x];
    break;

  // SHL Vx {, Vy } - 8xye
  case 0x0e:
    chip8->registers.V[0x0f] = chip8->registers.V[x] & 0b10000000;
    chip8->registers.V[x] = chip8->registers.V[x] * 2;
    break;
  }
}

static char chip8_wait_for_keypress(struct chip8 *chip8)
{
  SDL_Event event;
  while(SDL_WaitEvent(&event))
  {
    if(event.type != SDL_KEYDOWN)
    {
      continue;
    }

    char c = event.key.keysym.sym;
    char chip8_key = chip8_keyboard_map(&chip8->keyboard, c);
    if (chip8_key != -1)
    {
      return chip8_key;
    }
    
  }

  return -1;
}

static void chip8_exec_extended_f(struct chip8 *chip8, unsigned short opcode)
{
  unsigned char x = (opcode >> 8) & 0x000f;

  switch(opcode & 0x00ff)
  {
  // LD Vx, DT
  case 0x07:
    chip8->registers.V[x] = chip8->registers.delay_timer;
    break;

  // LD Vx, K
  case 0x0A:
    {
      char pressed_key = chip8_wait_for_keypress(chip8);
      chip8->registers.V[x] = pressed_key;
    }
    break;

  // LD DT, Vx
  case 0x15:
    chip8->registers.delay_timer = chip8->registers.V[x];
    break;

  // LD ST, Vx
  case 0x18:
    chip8->registers.sound_timer = chip8->registers.V[x];
    break;

  // ADD I, Vx
  case 0x1e:
    chip8->registers.I += chip8->registers.V[x];
    break;

  // LD F, Vx
  case 0x29:
    chip8->registers.I = chip8->registers.V[x] * CHIP8_DEFAULT_SPRITE_HEIGHT;
    break;

  // LD B, Vx
  case 0x33:
    {
      unsigned char hundreds = chip8->registers.V[x] / 100;
      unsigned char tens = (chip8->registers.V[x] / 10) % 10;
      unsigned char units = chip8->registers.V[x] % 10;
      chip8_memory_set(&chip8->memory, chip8->registers.I, hundreds);
      chip8_memory_set(&chip8->memory, chip8->registers.I+1, tens);
      chip8_memory_set(&chip8->memory, chip8->registers.I+2, units);
    }
    break;

  // LD [I], Vx
  case 0x55:
    {
      for(int i = 0; i <= x; i++)
      {
        chip8_memory_set(&chip8->memory, chip8->registers.I + i, chip8->registers.V[i]);
      }
    }
    break;

  // LD Vx, [I]
  case 0x65:
    {    
      for(int i = 0; i <= x; i++)
      {
        chip8->registers.V[i] = chip8_memory_get(&chip8->memory, chip8->registers.I + i);
      }
    }
    break;

  }
}

static void chip8_exec_extended(struct chip8 *chip8, unsigned short opcode)
{
  unsigned short nnn = opcode & 0x0fff;
  unsigned char x = (opcode >> 8) & 0x000f;
  unsigned char y = (opcode >> 4) & 0x000f;
  unsigned char kk = opcode & 0x00ff;
  unsigned char n = opcode & 0x000f;

  switch (opcode & 0xf000)
  {
  //JP addr, 1nnn jump to the location nnn
  case 0x1000:
    chip8->registers.PC = nnn;
    break;

  // CALL addr, call subroutine at the location nnn
  case 0x2000:
    chip8_stack_push(chip8, chip8->registers.PC);
    chip8->registers.PC = nnn;
    break;

  // SE Vx, byte - 3xkk skip next instruction if Vx == kk
  case 0x3000:
    if (chip8->registers.V[x] == kk)
    {
      chip8->registers.PC += 2;
    }
    break;

  // SNE Vx, byte - 4xkk skip next instruction if Vx != kk
  case 0x4000:
    if (chip8->registers.V[x] != kk)
    {
      chip8->registers.PC += 2;
    }
    break;

  // SE Vx, Vy - skip next instruction if Vx == Vy
  case 0x5000:
    if (chip8->registers.V[x] == chip8->registers.V[y])
    {
      chip8->registers.PC += 2;
    }
    break;

  // LD Vx, byte - 6xkk, put the value kk into Vx
  case 0x6000:
    chip8->registers.V[x] = kk;
    break;

  // ADD Vx, byte - 7xkk, adds kk to the value of Vx
  case 0x7000:
    chip8->registers.V[x] += kk;
    break;

  case 0x8000:
    chip8_exec_extended_eight(chip8, opcode);
    break;

  // SNE Vx, Vy - 9xy0 skip next instruction if Vx != Vy
  case 0x9000:
    if (chip8->registers.V[x] != chip8->registers.V[y])
    {
      chip8->registers.PC += 2;
    }
    break;

  // LD I, addr - Annn, register I = nnn
  case 0xA000:
    chip8->registers.I = nnn;
    break;

  // JP V0, addr - Bnnn, jump to location nnn + V0
  case 0xB000:
    chip8->registers.PC = nnn + chip8->registers.V[0x00];
    break;

  // RND Vx, byte - Cnnn, set Vx = random byte AND kk.
  case 0xC000:
    srand(clock());

    chip8->registers.V[x] = (rand() % 255) & kk;
    break;

  // DRW Vx, Vy, nibble - display n-byte sprite, starting at memory location I at (Vx, Vy)
  case 0xD000:
    { 
      const char* sprite = (const char*) &chip8->memory.memory[chip8->registers.I];
      chip8->registers.V[0x0f] = chip8_screen_draw_sprite(&chip8->screen, chip8->registers.V[x], chip8->registers.V[y], sprite, n);
    }
    break;

  case 0xE000:
    switch(opcode & 0x00ff)
    {
    case 0x9e:
      if (chip8_keyboard_is_down(&chip8->keyboard, chip8->registers.V[x])) 
      {
        chip8->registers.PC += 2;
      }
      break;
    case 0xa1:
      if (!chip8_keyboard_is_down(&chip8->keyboard, chip8->registers.V[x])) 
      {
        chip8->registers.PC += 2;
      }
      break;
    }

    break;

  case 0xF000:
    chip8_exec_extended_f(chip8, opcode);
    break;  
  
  }
}

void chip8_exec(struct chip8 *chip8, unsigned short opcode)
{
  switch (opcode)
  {

  // CLS: clear the display
  case 0x00E0:
    chip8_screen_clear(&chip8->screen);
    break;

  // RET: return from the subroutine
  case 0x00ee:
    chip8->registers.PC = chip8_stack_pop(chip8);
    break;

  default:
    chip8_exec_extended(chip8, opcode);
    break;
  }
}