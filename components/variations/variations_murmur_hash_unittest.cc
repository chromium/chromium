// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_murmur_hash.h"

#include <limits>
#include <vector>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/smhasher/src/MurmurHash3.h"

namespace variations {
namespace internal {

TEST(VariationsMurmurHashTest, StringToLE32) {
  EXPECT_EQ(std::vector<uint32_t>(),
            VariationsMurmurHash::StringToLE32(""));
  EXPECT_EQ(std::vector<uint32_t>({0x00000061}),
            VariationsMurmurHash::StringToLE32("a"));
  EXPECT_EQ(std::vector<uint32_t>({0x00006261}),
            VariationsMurmurHash::StringToLE32("ab"));
  EXPECT_EQ(std::vector<uint32_t>({0x00636261}),
            VariationsMurmurHash::StringToLE32("abc"));
  EXPECT_EQ(std::vector<uint32_t>({0x64636261}),
            VariationsMurmurHash::StringToLE32("abcd"));
  EXPECT_EQ(std::vector<uint32_t>({0x64636261, 0x00000065}),
            VariationsMurmurHash::StringToLE32("abcde"));
  EXPECT_EQ(std::vector<uint32_t>({0x64636261, 0x00006665}),
            VariationsMurmurHash::StringToLE32("abcdef"));
}

// The tests inside this #if compare VariationsMurmurHash to the reference
// implementation, MurmurHash3_x86_32, which only works on little-endian.
#if defined(ARCH_CPU_LITTLE_ENDIAN)

// Compare VariationsMurmurHash::Hash to MurmurHash3_x86_32 for every prefix of
// |data|, from the empty string to all of |data|.
TEST(VariationsMurmurHashTest, Hash) {
  // Random bytes generated manually and hard-coded for reproducability
  const std::vector<uint32_t> data({
      2704264845, 2929902289, 1679431515, 1427187834, 1300338468,
       576307953, 1209988079, 1918627109, 3926412991,   74087765});

  size_t max_size = data.size() * sizeof(uint32_t);
  for (size_t size = 0; size <= max_size; size++) {
    uint32_t expected;
    MurmurHash3_x86_32(data.data(), size, /*seed=*/0, &expected);
    EXPECT_EQ(expected, VariationsMurmurHash::Hash(data, size))
        << "size=" << size;
  }
}

TEST(VariationsMurmurHashTest, Hash16) {
  // Pick some likely edge case values.
  constexpr uint32_t max32 = std::numeric_limits<uint32_t>::max();
  uint32_t seeds[] = {
    0, max32 / 2 - 1, max32 - 2,
    1, max32 / 2,     max32 - 1,
    2, max32 / 2 + 1, max32};

  constexpr uint16_t max16 = std::numeric_limits<uint16_t>::max();
  uint16_t data[] = {
    0, max16 / 2 - 1, max16 - 2,
    1, max16 / 2,     max16 - 1,
    2, max16 / 2 + 1, max16};

  for (uint32_t seed : seeds) {
    for (uint16_t datum : data) {
      uint32_t expected;
      MurmurHash3_x86_32(&datum, sizeof(datum), seed, &expected);
      EXPECT_EQ(expected, VariationsMurmurHash::Hash16(seed, datum))
          << "seed=" << seed << ", datum=" << datum;
    }
  }
}

#endif  // defined(ARCH_CPU_LITTLE_ENDIAN)

}  // namespace internal
}  // namespace variations
