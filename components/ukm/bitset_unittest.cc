// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/bitset.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

TEST(UkmBitSet, BitSet) {
  constexpr size_t kBitSetSize = 32;

  // Create a bitset and verify that it is initially empty.
  BitSet bitset1(kBitSetSize);
  for (size_t i = 0; i < kBitSetSize; ++i) {
    EXPECT_FALSE(bitset1.Contains(i));
  }
  EXPECT_TRUE(bitset1.Serialize().empty());

  // Add some elements to the bitset, and verify they can be looked up.
  bitset1.Add(0);
  bitset1.Add(14);
  for (size_t i = 0; i < kBitSetSize; ++i) {
    EXPECT_EQ(bitset1.Contains(i), i == 0 || i == 14);
  }

  // Serialize the bitset. The trailing zeros should be optimized away, and the
  // resulting output should fit in 2 bytes (0b01000000 and 0b00000001).
  std::string serialized = bitset1.Serialize();
  EXPECT_EQ(serialized.size(), 2U);
  EXPECT_EQ(static_cast<uint8_t>(serialized[0]), 0b01000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[1]), 0b00000001);

  // Create a new bitset using the serialized data and verify that it contains
  // the same data.
  BitSet bitset2(kBitSetSize, serialized);
  for (size_t i = 0; i < kBitSetSize; ++i) {
    EXPECT_EQ(bitset1.Contains(i), bitset2.Contains(i));
  }

  // Do some last few checks for good measure.
  bitset2.Add(31);
  // Adding the same element a second time should have no impact.
  bitset2.Add(31);
  for (size_t i = 0; i < kBitSetSize; ++i) {
    EXPECT_EQ(bitset2.Contains(i), i == 0 || i == 14 || i == 31);
  }
  serialized = bitset2.Serialize();
  EXPECT_EQ(serialized.size(), 4U);
  EXPECT_EQ(static_cast<uint8_t>(serialized[0]), 0b10000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[1]), 0b00000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[2]), 0b01000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[3]), 0b00000001);
}

}  // namespace ukm
