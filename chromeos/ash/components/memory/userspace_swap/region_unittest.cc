// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/userspace_swap/region.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {
namespace userspace_swap {

// This test validates the behavior of CalculateOverlap when a region and a
// range don't overlap at all.
TEST(Region, OverlapNone) {
  Region region(1000, 100);  // Region from [1000, 1100]

  // Calculate no overlap ahead of the region.
  Region range(900, 99);  // Range from [900,1000]
  RegionOverlap overlap = region.CalculateRegionOverlap(range);
  ASSERT_FALSE(overlap.before);
  ASSERT_FALSE(overlap.intersection);
  ASSERT_FALSE(overlap.after);

  // Calculate no overlap beyond the region.
  Region range2(1101, 99);  // Range from [1101, 1200]
  RegionOverlap overlap2 = region.CalculateRegionOverlap(range2);
  ASSERT_FALSE(overlap2.before);
  ASSERT_FALSE(overlap2.intersection);
  ASSERT_FALSE(overlap2.after);
}

// This test validates the behavior of CalculateOverlap when range fully covers
// the region.
TEST(Region, OverlapFull) {
  Region region(1000, 100);  // Region from [1000, 1100]
  Region range(900, 300);    // Range from [900,1200]

  // And the intersection->should be the same as region.
  RegionOverlap overlap = region.CalculateRegionOverlap(range);
  ASSERT_FALSE(overlap.before);
  ASSERT_FALSE(overlap.after);
  ASSERT_TRUE(overlap.intersection);
  EXPECT_EQ(overlap.intersection->address, region.address);
  EXPECT_EQ(overlap.intersection->length, region.length);
}

// This validates the behavior when the range overlaps from the start of the
// range and the end piece of region remains.
TEST(Region, OverlapEndRemains) {
  Region region(1000, 100);  // Region from [1000, 1100]

  Region range(900, 150);  // Range from [900,1050]
  RegionOverlap overlap = region.CalculateRegionOverlap(range);

  ASSERT_FALSE(overlap.before);
  ASSERT_TRUE(overlap.intersection);
  ASSERT_TRUE(overlap.after);
  EXPECT_EQ(overlap.intersection->address, region.address);
  EXPECT_EQ(overlap.intersection->length, 50u);
  EXPECT_EQ(overlap.after->length, 50u);
}

// This validates the behavior when the range overlaps from the end of the
// range and the start piece of region remains.
TEST(Region, OverlapStartRemains) {
  Region region(1000, 100);  // Region from [1000, 1100]

  Region range(1050, 150);  // Range from [1050,1200]
  RegionOverlap overlap = region.CalculateRegionOverlap(range);

  ASSERT_FALSE(overlap.after);
  ASSERT_TRUE(overlap.intersection);
  ASSERT_TRUE(overlap.before);
  EXPECT_EQ(overlap.intersection->address, region.address + 50);
  EXPECT_EQ(overlap.intersection->length, 50u);
  EXPECT_EQ(overlap.before->length, 50u);
}

// This test validates a range which is fully within a region.
TEST(Region, OverlapWithinRegion) {
  Region region(1000, 100);  // Region from [1000, 1100]

  Region range(1050, 25);  // Range from [1050,1075]
  RegionOverlap overlap = region.CalculateRegionOverlap(range);

  ASSERT_TRUE(overlap.intersection);
  ASSERT_TRUE(overlap.before);
  ASSERT_TRUE(overlap.after);
  EXPECT_EQ(overlap.intersection->address, region.address + 50);
  EXPECT_EQ(overlap.intersection->length, 25u);
  EXPECT_EQ(overlap.before->length, 50u);
  EXPECT_EQ(overlap.after->length, 25u);
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
