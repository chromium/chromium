// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_cpu/core_times.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace system_cpu {

using CoreTimesTest = testing::Test;

TEST_F(CoreTimesTest, TimeUtilizationBalanced) {
  CoreTimes baseline({105, 205, 305, 405, 505, 605, 705, 805, 905, 1005});

  std::vector<CoreTimes> test_cases = {
      // Individual components get summed up correctly.
      CoreTimes({115, 205, 305, 415, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 215, 305, 415, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 315, 415, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 415, 505, 615, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 415, 505, 605, 715, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 415, 505, 605, 705, 815, 905, 1005}),
      // Ignored components are not added to the sum.
      CoreTimes({115, 205, 305, 415, 595, 605, 705, 805, 905, 1005}),
      CoreTimes({115, 205, 305, 415, 505, 605, 705, 805, 995, 1005}),
      CoreTimes({115, 205, 305, 415, 505, 605, 705, 805, 905, 1095}),
      // Negative changes in ignored components are not flagged.
      CoreTimes({115, 205, 305, 415, 500, 605, 705, 805, 905, 1005}),
      CoreTimes({115, 205, 305, 415, 505, 605, 705, 805, 900, 1005}),
      CoreTimes({115, 205, 305, 415, 505, 605, 705, 805, 905, 1000}),
  };

  for (const CoreTimes& times : test_cases) {
    SCOPED_TRACE(times.times_);
    EXPECT_EQ(0.5, times.TimeUtilization(baseline));
  }
}

TEST_F(CoreTimesTest, TimeUtilizationEmptyRange) {
  CoreTimes baseline({105, 205, 305, 405, 505, 605, 705, 805, 905, 1005});

  std::vector<CoreTimes> test_cases = {
      // Identical times.
      CoreTimes({105, 205, 305, 405, 505, 605, 705, 805, 905, 1005}),
      // Changes in ignored components.
      CoreTimes({105, 205, 305, 405, 515, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 405, 505, 605, 705, 805, 915, 1005}),
      CoreTimes({105, 205, 305, 405, 505, 605, 705, 805, 905, 1015}),
  };

  for (const CoreTimes& times : test_cases) {
    SCOPED_TRACE(times.times_);
    EXPECT_EQ(-1, times.TimeUtilization(baseline));
  }
}

TEST_F(CoreTimesTest, TimeUtilizationNegativeRange) {
  CoreTimes baseline({105, 205, 305, 405, 505, 605, 705, 805, 905, 1005});

  std::vector<CoreTimes> test_cases = {
      CoreTimes({100, 205, 305, 415, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 200, 305, 415, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 300, 415, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({115, 205, 305, 400, 505, 605, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 415, 505, 600, 705, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 415, 505, 605, 700, 805, 905, 1005}),
      CoreTimes({105, 205, 305, 415, 505, 605, 705, 800, 905, 1005}),
  };

  for (const CoreTimes& times : test_cases) {
    SCOPED_TRACE(times.times_);
    EXPECT_EQ(-1, times.TimeUtilization(baseline));
  }
}

TEST_F(CoreTimesTest, TimeUtilizationComputation) {
  CoreTimes baseline({105, 205, 305, 405, 505, 605, 705, 805, 905, 1005});

  struct TestCase {
    CoreTimes times;
    double utilization;
  };

  std::vector<TestCase> test_cases = {
      // No idling.
      {CoreTimes({115, 205, 305, 405, 505, 605, 705, 805, 905, 1005}), 1.0},
      {CoreTimes({115, 215, 305, 405, 505, 605, 705, 805, 905, 1005}), 1.0},
      {CoreTimes({115, 215, 315, 405, 505, 605, 705, 805, 905, 1005}), 1.0},
      {CoreTimes({115, 215, 315, 405, 505, 615, 705, 805, 905, 1005}), 1.0},
      {CoreTimes({115, 215, 315, 405, 505, 615, 715, 805, 905, 1005}), 1.0},
      {CoreTimes({115, 215, 315, 405, 505, 615, 715, 815, 905, 1005}), 1.0},
      // All idling.
      {CoreTimes({105, 205, 305, 415, 505, 605, 705, 805, 905, 1005}), 0.0},
      // Fractions
      {CoreTimes({115, 225, 305, 415, 505, 605, 705, 805, 905, 1005}), 0.75},
      {CoreTimes({115, 225, 345, 415, 505, 605, 705, 805, 905, 1005}), 0.875},
      {CoreTimes({115, 205, 305, 435, 505, 605, 705, 805, 905, 1005}), 0.25},
      {CoreTimes({115, 205, 305, 475, 505, 605, 705, 805, 905, 1005}), 0.125},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.times.times_);
    EXPECT_EQ(test_case.utilization, test_case.times.TimeUtilization(baseline));
  }
}

}  // namespace system_cpu
