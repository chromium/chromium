// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_murmur_hash.h"

#include <string.h>

#include <string_view>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"

namespace variations {
namespace internal {

// static
std::vector<uint32_t> VariationsMurmurHash::StringToLE32(
    std::string_view string) {
  auto data = base::as_byte_span(string);
  const size_t data_size = data.size();
  const size_t full_words = data_size / 4u;
  // Include any partial word at the end of the `data` buffer.
  const size_t total_words = (data_size + 3u) / 4u;

  std::vector<uint32_t> words(total_words, 0u);
  // Copy the words that are fully present in the `data` buffer.
  for (size_t i = 0u; i < full_words; ++i) {
    words[i] = base::U32FromLittleEndian(data.subspan(4u * i).first<4u>());
  }
  // Copy the last partial-word from the end of the `data` buffer, padding
  // the tail (MSBs) with 0.
  if (total_words > full_words) {
    const size_t rem = data_size % 4u;
    std::array<uint8_t, 4u> bytes = {};
    base::span(bytes).copy_prefix_from(data.last(rem));
    words[full_words] = base::U32FromLittleEndian(bytes);
  }
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
