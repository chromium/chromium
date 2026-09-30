// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/sha1/sha1.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <string_view>

// chrome_elf has its own SHA-1 implementation, to avoid depending on //base
// (base/hash/sha1.h) and BoringSSL. chrome_elf.dll is loaded very early, and
// SHA1HashString() is called by its NtMapViewOfSection hook, on every DLL load.
//
// SHA-1 is specified by FIPS PUB 180-4, whose sections are referenced below:
// https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf

namespace elf_sha1 {
namespace {

// A hash value, as five 32-bit words.
using Hash = std::array<uint32_t, 5>;

// A 512-bit message block.
using Block = std::array<uint8_t, 64>;

// Section 5.3.1.
constexpr Hash kInitialHash = {
    0x67452301,  // H0
    0xefcdab89,  // H1
    0x98badcfe,  // H2
    0x10325476,  // H3
    0xc3d2e1f0,  // H4
};

// Section 4.1.1: Returns f_t(x, y, z).
uint32_t RoundFunction(size_t t, uint32_t x, uint32_t y, uint32_t z) {
  if (t < 20) {
    return (x & y) | (~x & z);  // Ch(x, y, z)
  }
  if (t < 40) {
    return x ^ y ^ z;  // Parity(x, y, z)
  }
  if (t < 60) {
    return (x & y) | (x & z) | (y & z);  // Maj(x, y, z)
  }
  return x ^ y ^ z;  // Parity(x, y, z)
}

// Section 4.2.1: Returns K_t.
uint32_t RoundConstant(size_t t) {
  if (t < 20) {
    return 0x5a827999;
  }
  if (t < 40) {
    return 0x6ed9eba1;
  }
  if (t < 60) {
    return 0x8f1bbcdc;
  }
  return 0xca62c1d6;
}

// Section 6.1.2: Updates `hash` with `block`.
void ProcessBlock(Hash& hash, const Block& block) {
  // 1. Prepare the message schedule `w`. Its first 16 words are the block,
  // read as big-endian.
  std::array<uint32_t, 80> w;
  for (size_t t = 0; t < 16; ++t) {
    w[t] = static_cast<uint32_t>(block[4 * t + 0]) << 24 |
           static_cast<uint32_t>(block[4 * t + 1]) << 16 |
           static_cast<uint32_t>(block[4 * t + 2]) << 8 |
           static_cast<uint32_t>(block[4 * t + 3]);
  }
  for (size_t t = 16; t < 80; ++t) {
    w[t] = std::rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
  }

  // 2. Initialize the working variables.
  auto [a, b, c, d, e] = hash;

  // 3. Run the 80 rounds.
  for (size_t t = 0; t < 80; ++t) {
    const uint32_t temp = std::rotl(a, 5) + RoundFunction(t, b, c, d) + e +
                          RoundConstant(t) + w[t];
    e = d;
    d = c;
    c = std::rotl(b, 30);
    b = a;
    a = temp;
  }

  // 4. Compute the intermediate hash value.
  hash[0] += a;
  hash[1] += b;
  hash[2] += c;
  hash[3] += d;
  hash[4] += e;
}

// Updates `hash` with the complete blocks of `message`, and returns the rest.
std::string_view ProcessCompleteBlocks(Hash& hash, std::string_view message) {
  while (message.size() >= Block().size()) {
    Block block;
    std::ranges::copy(message.substr(0, block.size()), block.begin());
    ProcessBlock(hash, block);
    message.remove_prefix(block.size());
  }
  return message;
}

// Section 5.1.1: Updates `hash` with the `rest` of the message, padded with a
// '1' bit, zeros, and the message length in bits as a 64-bit big-endian integer
// in the last 8 bytes.
void ProcessPaddedRest(Hash& hash, std::string_view rest, size_t message_size) {
  Block block = {};
  std::ranges::copy(rest, block.begin());
  block[rest.size()] = 0b1000'0000;
  if (rest.size() >= block.size() - 8) {
    // There is no room left for the length. It goes into an extra block.
    ProcessBlock(hash, block);
    block = {};
  }
  const uint64_t length_in_bits = uint64_t{message_size} * 8;
  for (size_t i = 0; i < 8; ++i) {
    block[block.size() - 1 - i] =
        static_cast<uint8_t>(length_in_bits >> (8 * i));
  }
  ProcessBlock(hash, block);
}

// Returns the message digest: the words of `hash`, serialized as big-endian.
Digest ToDigest(const Hash& hash) {
  Digest digest;
  for (size_t i = 0; i < hash.size(); ++i) {
    digest[4 * i + 0] = static_cast<uint8_t>(hash[i] >> 24);
    digest[4 * i + 1] = static_cast<uint8_t>(hash[i] >> 16);
    digest[4 * i + 2] = static_cast<uint8_t>(hash[i] >> 8);
    digest[4 * i + 3] = static_cast<uint8_t>(hash[i]);
  }
  return digest;
}

}  // namespace

Digest SHA1HashString(const std::string& str) {
  Hash hash = kInitialHash;
  const std::string_view rest = ProcessCompleteBlocks(hash, str);
  ProcessPaddedRest(hash, rest, str.size());
  return ToDigest(hash);
}

}  // namespace elf_sha1
