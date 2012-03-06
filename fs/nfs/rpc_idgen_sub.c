/****************************************************************************
 * fs/nfs/rpc_subr.c
 * Non-repeating ID generation covering an almost maximal 32 bit range.
 *
 *   Copyright (C) 2012 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2012 Jose Pablo Rojas Vargas. All rights reserved.
 *   Author: Jose Pablo Rojas Vargas <jrojas@nx-engineering.com>
 *
 * Leveraged from OpenBSD:
 *
 *   Copyright (c) 2008 Damien Miller <djm@mindrot.org>
 *   Based on public domain SKIP32 by Greg Rose.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#include"rpc_idgen.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const uint8_t ftable[256] = 
{
  0xa3, 0xd7, 0x09, 0x83, 0xf8, 0x48, 0xf6, 0xf4,
  0xb3, 0x21, 0x15, 0x78, 0x99, 0xb1, 0xaf, 0xf9,
  0xe7, 0x2d, 0x4d, 0x8a, 0xce, 0x4c, 0xca, 0x2e,
  0x52, 0x95, 0xd9, 0x1e, 0x4e, 0x38, 0x44, 0x28,
  0x0a, 0xdf, 0x02, 0xa0, 0x17, 0xf1, 0x60, 0x68,
  0x12, 0xb7, 0x7a, 0xc3, 0xe9, 0xfa, 0x3d, 0x53,
  0x96, 0x84, 0x6b, 0xba, 0xf2, 0x63, 0x9a, 0x19,
  0x7c, 0xae, 0xe5, 0xf5, 0xf7, 0x16, 0x6a, 0xa2,
  0x39, 0xb6, 0x7b, 0x0f, 0xc1, 0x93, 0x81, 0x1b,
  0xee, 0xb4, 0x1a, 0xea, 0xd0, 0x91, 0x2f, 0xb8,
  0x55, 0xb9, 0xda, 0x85, 0x3f, 0x41, 0xbf, 0xe0,
  0x5a, 0x58, 0x80, 0x5f, 0x66, 0x0b, 0xd8, 0x90,
  0x35, 0xd5, 0xc0, 0xa7, 0x33, 0x06, 0x65, 0x69,
  0x45, 0x00, 0x94, 0x56, 0x6d, 0x98, 0x9b, 0x76,
  0x97, 0xfc, 0xb2, 0xc2, 0xb0, 0xfe, 0xdb, 0x20,
  0xe1, 0xeb, 0xd6, 0xe4, 0xdd, 0x47, 0x4a, 0x1d,
  0x42, 0xed, 0x9e, 0x6e, 0x49, 0x3c, 0xcd, 0x43,
  0x27, 0xd2, 0x07, 0xd4, 0xde, 0xc7, 0x67, 0x18,
  0x89, 0xcb, 0x30, 0x1f, 0x8d, 0xc6, 0x8f, 0xaa,
  0xc8, 0x74, 0xdc, 0xc9, 0x5d, 0x5c, 0x31, 0xa4,
  0x70, 0x88, 0x61, 0x2c, 0x9f, 0x0d, 0x2b, 0x87,
  0x50, 0x82, 0x54, 0x64, 0x26, 0x7d, 0x03, 0x40,
  0x34, 0x4b, 0x1c, 0x73, 0xd1, 0xc4, 0xfd, 0x3b,
  0xcc, 0xfb, 0x7f, 0xab, 0xe6, 0x3e, 0x5b, 0xa5,
  0xad, 0x04, 0x23, 0x9c, 0x14, 0x51, 0x22, 0xf0,
  0x29, 0x79, 0x71, 0x7e, 0xff, 0x8c, 0x0e, 0xe2,
  0x0c, 0xef, 0xbc, 0x72, 0x75, 0x6f, 0x37, 0xa1,
  0xec, 0xd3, 0x8e, 0x62, 0x8b, 0x86, 0x10, 0xe8,
  0x08, 0x77, 0x11, 0xbe, 0x92, 0x4f, 0x24, 0xc5,
  0x32, 0x36, 0x9d, 0xcf, 0xf3, 0xa6, 0xbb, 0xac,
  0x5e, 0x6c, 0xa9, 0x13, 0x57, 0x25, 0xb5, 0xe3,
  0xbd, 0xa8, 0x3a, 0x01, 0x05, 0x59, 0x2a, 0x46
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint16_t idgen32_g(uint8_t * key, int k, uint16_t w)
{
  uint8_t g1, g2, g3, g4, g5, g6;
  unsigned int o = k * 4;

  g1 = (w >> 8) & 0xff;
  g2 = w & 0xff;

  g3 = ftable[g2 ^ key[o++ & (IDGEN32_KEYLEN - 1)]] ^ g1;
  g4 = ftable[g3 ^ key[o++ & (IDGEN32_KEYLEN - 1)]] ^ g2;
  g5 = ftable[g4 ^ key[o++ & (IDGEN32_KEYLEN - 1)]] ^ g3;
  g6 = ftable[g5 ^ key[o++ & (IDGEN32_KEYLEN - 1)]] ^ g4;

  return (g5 << 8) | g6;
}

static uint32_t idgen32_permute(uint8_t key[IDGEN32_KEYLEN], uint32_t in)
{
  unsigned int i, r;
  uint16_t wl, wr;

  wl = (in >> 16) & 0x7fff;
  wr = in & 0xffff;

  /* Doubled up rounds, with an odd round at the end to swap */
  for (i = r = 0; i < IDGEN32_ROUNDS / 2; ++i)
    {
      wr ^= (idgen32_g(key, r, wl) ^ r);
      r++;
      wl ^= (idgen32_g(key, r, wr) ^ r) & 0x7fff;
      r++;
    }
  wr ^= (idgen32_g(key, r, wl) ^ r);

  return (wl << 16) | wr;
}

static void idgen32_rekey(struct idgen32_ctx *ctx)
{
  srand(time(NULL));
  ctx->id_counter = 0;
  ctx->id_hibit ^= 0x80000000;
  // ctx->id_offset = arc4random();
  ctx->id_offset = rand();
  // arc4random_buf(ctx->id_key, sizeof(ctx->id_key));
  ctx->id_key[0] = rand();
  ctx->id_key[1] = rand();
  ctx->id_key[2] = rand();
  ctx->id_key[3] = rand();
  ctx->id_key[4] = rand();
  ctx->id_key[5] = rand();
  ctx->id_key[6] = rand();
  ctx->id_key[7] = rand();
  ctx->id_key[8] = rand();
  ctx->id_key[9] = rand();
  ctx->id_key[10] = rand();
  ctx->id_key[11] = rand();
  ctx->id_key[12] = rand();
  ctx->id_key[13] = rand();
  ctx->id_key[14] = rand();
  ctx->id_key[15] = rand();
  ctx->id_key[16] = rand();
  ctx->id_key[17] = rand();
  ctx->id_key[18] = rand();
  ctx->id_key[19] = rand();
  ctx->id_key[20] = rand();
  ctx->id_key[21] = rand();
  ctx->id_key[22] = rand();
  ctx->id_key[23] = rand();
  ctx->id_key[24] = rand();
  ctx->id_key[25] = rand();
  ctx->id_key[26] = rand();
  ctx->id_key[27] = rand();
  ctx->id_key[28] = rand();
  ctx->id_key[29] = rand();
  ctx->id_key[30] = rand();
  ctx->id_key[31] = rand();
  ctx->id_rekey_time = IDGEN32_REKEY_TIME;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void idgen32_init(struct idgen32_ctx *ctx)
{
  srand(time(NULL));
  memset(ctx, 0, sizeof(*ctx));
  ctx->id_hibit = rand() & 0x80000000;
  idgen32_rekey(ctx);
}

uint32_t idgen32(struct idgen32_ctx *ctx)
{
  uint32_t ret;

  /* Avoid emitting a zero ID as they often have special meaning */
  do
    {
      ret = idgen32_permute(ctx->id_key, ctx->id_offset + ctx->id_counter++);

      /* Rekey a little early to avoid "card counting" attack */
      if (ctx->id_counter > IDGEN32_REKEY_LIMIT || ctx->id_rekey_time > 0)
        idgen32_rekey(ctx);
    }
  while (ret == 0);

  return ret | ctx->id_hibit;
}
