// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

base::Optional<AllocatorSettings> GetAllocatorSettings(
    const base::Feature& feature,
    bool boost_sampling);

namespace {

constexpr size_t kLoopIterations = 100;
const base::Feature kTestFeature1{"GwpAsanTestFeature1",
                                  base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kTestFeature2{"GwpAsanTestFeature2",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Tries to enable hooking with the given process sampling parameters
// kLoopIterations times and return the number of times hooking was enabled.
size_t processSamplingTest(const char* process_sampling,
                           const char* process_sampling_boost) {
  std::map<std::string, std::string> parameters;
  parameters["ProcessSamplingProbability"] = process_sampling;
  if (process_sampling_boost)
    parameters["ProcessSamplingBoost2"] = process_sampling_boost;

  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(kTestFeature1, parameters);

  size_t enabled = 0;
  for (size_t i = 0; i < kLoopIterations; i++) {
    if (GetAllocatorSettings(kTestFeature1, process_sampling_boost != nullptr))
      enabled++;
  }

  return enabled;
}

// Enables hooking kLoopIterations times with the given allocation sampling
// parameters and returns the allocation sampling frequencies hooking was
// enabled with.
std::set<size_t> allocationSamplingTest(
    const char* allocation_sampling_multiplier,
    const char* allocation_sampling_range) {
  std::map<std::string, std::string> parameters;
  parameters["ProcessSamplingProbability"] = "1.0";
  parameters["AllocationSamplingMultiplier"] = allocation_sampling_multiplier;
  parameters["AllocationSamplingRange"] = allocation_sampling_range;

  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(kTestFeature2, parameters);

  std::set<size_t> frequencies;
  for (size_t i = 0; i < kLoopIterations; i++) {
    if (auto settings = GetAllocatorSettings(kTestFeature2, false))
      frequencies.insert(settings->sampling_frequency);
  }

  return frequencies;
}

}  // namespace

TEST(GwpAsanTest, ProcessSamplingWorks) {
  EXPECT_EQ(processSamplingTest("1.0", nullptr), kLoopIterations);
  EXPECT_EQ(processSamplingTest("1.0", "100000"), kLoopIterations);
  EXPECT_EQ(processSamplingTest("0.01", "100"), kLoopIterations);

  EXPECT_EQ(processSamplingTest("0.0", nullptr), 0U);
  EXPECT_EQ(processSamplingTest("0.0", "100000"), 0U);

  size_t num_enabled = processSamplingTest("0.5", nullptr);
  EXPECT_GT(num_enabled, 0U);
  EXPECT_LT(num_enabled, kLoopIterations);
  num_enabled = processSamplingTest("0.01", "50");
  EXPECT_GT(num_enabled, 0U);
  EXPECT_LT(num_enabled, kLoopIterations);
}

TEST(GwpAsanTest, AllocationSamplingWorks) {
  std::set<size_t> frequencies = allocationSamplingTest("1000", "1");
  EXPECT_THAT(frequencies, testing::ElementsAre(1000));

  frequencies = allocationSamplingTest("1000", "64");
  EXPECT_GT(frequencies.size(), 1U);
  for (const size_t freq : frequencies) {
    EXPECT_GE(freq, 1000U);
    EXPECT_LE(freq, 64000U);
  }
}

}  // namespace internal
}  // namespace gwp_asan
