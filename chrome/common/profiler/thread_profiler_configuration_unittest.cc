// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_configuration.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ThreadProfilerConfigurationTest, ChooseVariationGroup) {
  std::initializer_list<ThreadProfilerConfiguration::Variation> variations = {
      {ThreadProfilerConfiguration::kProfileEnabled, 0.1},
      {ThreadProfilerConfiguration::kProfileDisabled, 99.9},
  };
  // randValue is multiplied by the sum of the weights. Dividing by 100 ensure
  // correct representation of the percentage value.
  EXPECT_EQ(ThreadProfilerConfiguration::ChooseVariationGroup(
                variations, /*randValue*/ 0.05 / 100.0),
            ThreadProfilerConfiguration::kProfileEnabled);
  EXPECT_EQ(ThreadProfilerConfiguration::ChooseVariationGroup(
                variations, /*randValue*/ 5.0 / 100.0),
            ThreadProfilerConfiguration::kProfileDisabled);
  EXPECT_EQ(ThreadProfilerConfiguration::ChooseVariationGroup(
                variations, /*randValue*/ 100.0 / 100.0),
            ThreadProfilerConfiguration::kProfileDisabled);
}
