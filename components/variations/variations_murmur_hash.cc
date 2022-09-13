// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_murmur_hash.h"

#include <string.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"

#if !(defined(ARCH_CPU_LITTLE_ENDIAN) || defined(ARCH_CPU_BIG_ENDIAN))
#error "unknown endianness"
#endif

namespace variations {
namespace internal {

// static
std::vector<uint32_t> VariationsMurmurHash::StringToLE32(
    base::StringPiece data) {
  const size_t data_size = data.size();
  const size_t word_num = (data_size + 3) / 4;  // data_size / 4, rounding up
  std::vector<uint32_t> words(word_num, 0);
  DCHECK_GE(words.size() * sizeof(uint32_t), data_size * sizeof(char));
  memcpy(words.data(), data.data(), data_size);

#if defined(ARCH_CPU_BIG_ENDIAN)
  // When packing chars into uint32_t, "abcd" may become 0x61626364 (big endian)
  // or 0x64636261 (little endian). If big endian, swap everything, so we get
  // the same values across platforms.
  for (auto it = words.begin(); it != words.end(); ++it)
    *it = base::ByteSwapToLE32(*it);
#endif  // defined(ARCH_CPU_BIG_ENDIAN)

  return words;
}

// static
uint32_t VariationsMurmurHash::Hash(const std::vector<uint32_t>& data,
                                    size_t length) {
  DCHECK_LE(length, data.size() * sizeof(uint32_t));
  uint32_t h1 = 0;

  // body
  size_t num_full_blocks = length / sizeof(uint32_t);
  for (size_t i = 0; i < num_full_blocks; i++) {
    uint32_t k1 = data[i];
    k1 *= c1;
    k1 = RotateLeft(k1, 15);
    k1 *= c2;
    h1 ^= k1;
    h1 = RotateLeft(h1, 13);
    h1 = h1 * 5 + 0xe6546b64;
  }

  // tail
  uint32_t k1 = 0;
  switch (length & 3) {
    case 3:
      k1 |= data[num_full_blocks] & 0xFF0000;
      [[fallthrough]];
    case 2:
      k1 |= data[num_full_blocks] & 0xFF00;
      [[fallthrough]];
    case 1:
      k1 |= data[num_full_blocks] & 0xFF;
  }
  k1 *= c1;
  k1 = RotateLeft(k1, 15);
  k1 *= c2;
  h1 ^= k1;

  // finalization
  h1 ^= length;
  h1 = FinalMix(h1);

  return h1;
}

}  // namespace internal
}  // namespace variations
