// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/peak_gpu_memory_tracker_util.h"

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class PeakGpuMemoryUtilTest
    : public ::testing::TestWithParam<SequenceLocation> {
 public:
  PeakGpuMemoryUtilTest() = default;
  ~PeakGpuMemoryUtilTest() override = default;

  void SetUp() override {}
  void TearDown() override {
    // Resets counters.
    SetSequenceNumberGeneratorForTesting(0, GetParam());
  }
};

// Tests that sequence numbers are generated according to SequenceLocation.
// Extreme values are tested below in separate tests.
TEST_P(PeakGpuMemoryUtilTest, NextSequenceNumber) {
  uint32_t sequence_num_generator = base::RandInt(1, INT_MAX - 1);
  SequenceLocation sequence_location = GetParam();

  SetSequenceNumberGeneratorForTesting(sequence_num_generator,
                                       sequence_location);

  uint32_t sequence_num_1 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_1),
            sequence_location);

  uint32_t sequence_num_2 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_2),
            sequence_location);

  uint32_t sequence_num_3 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_3),
            sequence_location);

  EXPECT_TRUE(sequence_num_1 != sequence_num_2 &&
              sequence_num_1 != sequence_num_3 &&
              sequence_num_2 != sequence_num_3);
}

// Tests that sequence numbers are generated according to SequenceLocation with
// sequence_num_generator as zero (extreme value).
TEST_P(PeakGpuMemoryUtilTest, SequenceNumberGeneratorZero) {
  uint32_t sequence_num_generator = 0;
  SequenceLocation sequence_location = GetParam();

  SetSequenceNumberGeneratorForTesting(sequence_num_generator,
                                       sequence_location);

  uint32_t sequence_num_1 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_1),
            sequence_location);

  uint32_t sequence_num_2 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_2),
            sequence_location);

  uint32_t sequence_num_3 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_3),
            sequence_location);

  EXPECT_TRUE(sequence_num_1 != sequence_num_2 &&
              sequence_num_1 != sequence_num_3 &&
              sequence_num_2 != sequence_num_3);
}

// Tests that sequence numbers are generated according to SequenceLocation with
// sequence_num_generator as INT_MAX (extreme value).
TEST_P(PeakGpuMemoryUtilTest, SequenceNumberGeneratorIntMax) {
  uint32_t sequence_num_generator = INT_MAX;
  SequenceLocation sequence_location = GetParam();

  SetSequenceNumberGeneratorForTesting(sequence_num_generator,
                                       sequence_location);

  uint32_t sequence_num_1 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_1),
            sequence_location);

  uint32_t sequence_num_2 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_2),
            sequence_location);

  uint32_t sequence_num_3 = GetNextSequenceNumber(sequence_location);
  EXPECT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num_3),
            sequence_location);

  EXPECT_TRUE(sequence_num_1 != sequence_num_2 &&
              sequence_num_1 != sequence_num_3 &&
              sequence_num_2 != sequence_num_3);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PeakGpuMemoryUtilTest,
                         ::testing::Values(SequenceLocation::kBrowserProcess,
                                           SequenceLocation::kGpuProcess));

}  // namespace viz
