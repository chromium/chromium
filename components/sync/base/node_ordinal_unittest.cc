// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/node_ordinal.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <vector>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const int64_t kTestValues[] = {0LL,
                               1LL,
                               -1LL,
                               2LL,
                               -2LL,
                               3LL,
                               -3LL,
                               0x79LL,
                               -0x79LL,
                               0x80LL,
                               -0x80LL,
                               0x81LL,
                               -0x81LL,
                               0xFELL,
                               -0xFELL,
                               0xFFLL,
                               -0xFFLL,
                               0x100LL,
                               -0x100LL,
                               0x101LL,
                               -0x101LL,
                               0xFA1AFELL,
                               -0xFA1AFELL,
                               0xFFFFFFFELL,
                               -0xFFFFFFFELL,
                               0xFFFFFFFFLL,
                               -0xFFFFFFFFLL,
                               0x100000000LL,
                               -0x100000000LL,
                               0x100000001LL,
                               -0x100000001LL,
                               0xFFFFFFFFFFLL,
                               -0xFFFFFFFFFFLL,
                               0x112358132134LL,
                               -0x112358132134LL,
                               0xFEFFBEEFABC1234LL,
                               -0xFEFFBEEFABC1234LL,
                               INT64_MAX,
                               INT64_MIN,
                               INT64_MIN + 1,
                               INT64_MAX - 1};

const size_t kNumTestValues = base::size(kTestValues);

// Convert each test value to an ordinal.  All ordinals should be
// valid.
TEST(NodeOrdinalTest, IsValid) {
  for (size_t i = 0; i < kNumTestValues; ++i) {
    const NodeOrdinal ordinal = Int64ToNodeOrdinal(kTestValues[i]);
    EXPECT_TRUE(ordinal.IsValid()) << "i = " << i;
  }
}

// Convert each test value to an ordinal.  All ordinals should have
// 8-byte strings, except for kint64min, which should have a 9-byte
// string.
TEST(NodeOrdinalTest, Size) {
  EXPECT_EQ(9U, Int64ToNodeOrdinal(std::numeric_limits<int64_t>::min())
                    .ToInternalValue()
                    .size());

  for (size_t i = 0; i < kNumTestValues; ++i) {
    if (kTestValues[i] == std::numeric_limits<int64_t>::min()) {
      continue;
    }
    const NodeOrdinal ordinal = Int64ToNodeOrdinal(kTestValues[i]);
    EXPECT_EQ(8U, ordinal.ToInternalValue().size()) << "i = " << i;
  }
}

// Convert each test value to an ordinal and back.  That resulting
// value should be equal to the original value.
TEST(NodeOrdinalTest, PositionToOrdinalToPosition) {
  for (size_t i = 0; i < kNumTestValues; ++i) {
    const int64_t expected_value = kTestValues[i];
    const NodeOrdinal ordinal = Int64ToNodeOrdinal(expected_value);
    const int64_t value = NodeOrdinalToInt64(ordinal);
    EXPECT_EQ(expected_value, value) << "i = " << i;
  }
}

template <typename T, typename LessThan = std::less<T>>
class IndexedLessThan {
 public:
  explicit IndexedLessThan(const T* values) : values_(values) {}

  bool operator()(int i1, int i2) {
    return less_than_(values_[i1], values_[i2]);
  }

 private:
  const T* values_;
  LessThan less_than_;
};

// Sort kTestValues by int64_t value and then sort it by NodeOrdinal
// value.  kTestValues should not already be sorted (by either
// comparator) and the two orderings should be the same.
TEST(NodeOrdinalTest, ConsistentOrdering) {
  NodeOrdinal ordinals[kNumTestValues];
  std::vector<int> original_ordering(kNumTestValues);
  std::vector<int> int64_ordering(kNumTestValues);
  std::vector<int> ordinal_ordering(kNumTestValues);
  for (size_t i = 0; i < kNumTestValues; ++i) {
    ordinals[i] = Int64ToNodeOrdinal(kTestValues[i]);
    original_ordering[i] = int64_ordering[i] = ordinal_ordering[i] = i;
  }

  std::sort(int64_ordering.begin(), int64_ordering.end(),
            IndexedLessThan<int64_t>(kTestValues));
  std::sort(ordinal_ordering.begin(), ordinal_ordering.end(),
            IndexedLessThan<NodeOrdinal, NodeOrdinal::LessThanFn>(ordinals));
  EXPECT_NE(original_ordering, int64_ordering);
  EXPECT_EQ(int64_ordering, ordinal_ordering);
}

// Create two NodeOrdinals and create another one between them.  It
// should lie halfway between them.
TEST(NodeOrdinalTest, CreateBetween) {
  const NodeOrdinal ordinal1("\1\1\1\1\1\1\1\1");
  const NodeOrdinal ordinal2("\1\1\1\1\1\1\1\3");
  EXPECT_EQ("\1\1\1\1\1\1\1\2",
            ordinal1.CreateBetween(ordinal2).ToInternalValue());
}

}  // namespace

}  // namespace syncer
