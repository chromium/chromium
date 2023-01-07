// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/pack_stack_trace.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

TEST(PackTest, TrivialExample) {
  constexpr size_t kTestEntries = 3;

  uintptr_t test[kTestEntries] = {1, 2, 3};
  uint8_t packed[8];
  uintptr_t unpacked[kTestEntries];

  const size_t packed_size = Pack(test, kTestEntries, packed, sizeof(packed));
  ASSERT_EQ(packed_size, 3U);
  // ZigzagEncode(1) == 2
  EXPECT_EQ(packed[0], 2U);
  EXPECT_EQ(packed[1], 2U);
  EXPECT_EQ(packed[2], 2U);
  EXPECT_EQ(Unpack(packed, packed_size, unpacked, kTestEntries), kTestEntries);
  EXPECT_EQ(unpacked[0], 1U);
  EXPECT_EQ(unpacked[1], 2U);
  EXPECT_EQ(unpacked[2], 3U);
}

TEST(PackTest, DecreasingSequence) {
  constexpr size_t kTestEntries = 3;

  uintptr_t test[kTestEntries] = {3, 2, 1};
  uint8_t packed[8];
  uintptr_t unpacked[kTestEntries];

  const size_t packed_size = Pack(test, kTestEntries, packed, sizeof(packed));
  ASSERT_EQ(packed_size, 3U);
  // ZigzagEncode(3) == 6
  // ZigzagEncode(-1) == 1
  EXPECT_EQ(packed[0], 6U);
  EXPECT_EQ(packed[1], 1U);
  EXPECT_EQ(packed[2], 1U);
  EXPECT_EQ(Unpack(packed, packed_size, unpacked, kTestEntries), kTestEntries);
  EXPECT_EQ(unpacked[0], 3U);
  EXPECT_EQ(unpacked[1], 2U);
  EXPECT_EQ(unpacked[2], 1U);
}

TEST(PackTest, MultibyteVarInts) {
  constexpr size_t kTestEntries = 1;

  uintptr_t test[kTestEntries] = {0x40};
  uint8_t packed[8];
  uintptr_t unpacked[kTestEntries];

  const size_t packed_size = Pack(test, kTestEntries, packed, sizeof(packed));
  ASSERT_EQ(packed_size, 2U);
  // ZigzagEncode(0x40) == 0x80
  EXPECT_EQ(packed[0], 0x80U);
  EXPECT_EQ(packed[1], 0x01U);
  EXPECT_EQ(Unpack(packed, packed_size, unpacked, kTestEntries), kTestEntries);
  EXPECT_EQ(unpacked[0], 0x40U);
}

TEST(PackTest, UnpackFailsOnOutOfBoundsVarInt) {
  uint8_t packed[11] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
                        0x80, 0x80, 0x80, 0x80, 0x00};
  uintptr_t unpacked[1];

  EXPECT_EQ(Unpack(packed, 11, unpacked, 1), 0U);
}

TEST(PackTest, UnpackFailsOnBufferTooSmall) {
  uint8_t packed[2] = {0x80, 0x00};
  uintptr_t unpacked[2];

  // Fail
  EXPECT_EQ(Unpack(packed, 1, unpacked, 1), 0U);
  // Success
  EXPECT_EQ(Unpack(packed, 2, unpacked, 1), 1U);
  EXPECT_EQ(Unpack(packed, 2, unpacked, 2), 1U);
}

TEST(PackTest, PackFailsOnBufferTooSmall) {
  uintptr_t test[] = {0x40, 0x41};
  uint8_t packed[4];

  EXPECT_EQ(Pack(test, 2, packed, 1), 0U);
  EXPECT_EQ(Pack(test, 2, packed, 2), 2U);
  EXPECT_EQ(Pack(test, 2, packed, 3), 3U);
  EXPECT_EQ(Pack(test, 2, packed, 4), 3U);
}

}  // namespace internal
}  // namespace gwp_asan
