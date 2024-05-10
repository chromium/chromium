// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

std::optional<AllocatorSettings> GetAllocatorSettingsImpl(
    const base::Feature& feature,
    bool boost_sampling,
    std::string_view process_type);
std::optional<AllocatorSettings> GetAllocatorSettings(
    const base::Feature& feature,
    bool boost_sampling,
    std::string_view process_type);

namespace {

constexpr size_t kLoopIterations = 100;
BASE_FEATURE(kTestFeature1,
             "GwpAsanTestFeature1",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2,
             "GwpAsanTestFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature3,
             "GwpAsanTestFeature3",
             base::FEATURE_ENABLED_BY_DEFAULT);

inline constexpr std::string_view kDummyProcessType =
    "assuredly not a real process type";

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
    if (GetAllocatorSettings(kTestFeature1, process_sampling_boost != nullptr,
                             kDummyProcessType)) {
      enabled++;
    }
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
    if (auto settings =
            GetAllocatorSettings(kTestFeature2, false, kDummyProcessType)) {
      frequencies.insert(settings->sampling_frequency);
    }
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

TEST(GwpAsanTest, GetDefaultAllocatorSettings) {
  std::map<std::string, std::string> empty_parameters;
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(kTestFeature3,
                                                    empty_parameters);

  const auto settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, kDummyProcessType);
  EXPECT_TRUE(settings.has_value());
}

TEST(GwpAsanTest, GetOutOfRangeAllocatorSettings) {
  std::map<std::string, std::string> bad_parameters;
  // Exceeds `MaxMetadata`, forcing failure.
  bad_parameters["MaxAllocations"] = "9999";
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(kTestFeature3,
                                                    bad_parameters);

  const auto settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, kDummyProcessType);
  EXPECT_FALSE(settings.has_value());
}

TEST(GwpAsanTest, GetProcessSpecificAllocatorSettings) {
  std::map<std::string, std::string> process_specific_parameters;
  // Set to weird and distinctive values.
  process_specific_parameters["BrowserMaxAllocations"] = "21";
  process_specific_parameters["RendererMaxAllocations"] = "22";

  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(
      kTestFeature3, process_specific_parameters);

  // The empty `process_type` string denotes the browser process.
  const auto browser_settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, "");
  EXPECT_TRUE(browser_settings.has_value());
  EXPECT_EQ(browser_settings->max_allocated_pages, 21ul);

  const auto renderer_settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, "renderer");
  EXPECT_TRUE(renderer_settings.has_value());
  EXPECT_EQ(renderer_settings->max_allocated_pages, 22ul);
}

TEST(GwpAsanTest, GetProcessSpecificAllocationSamplingMultiplier) {
  std::map<std::string, std::string> parameters;

  // This parameter is never overridden, hence common to all processes.
  parameters["AllocationSamplingRange"] = "1";

  // Since the range is set to 1, the multiplier fully determines
  // the sampling frequency:
  // 2 * (1 ** RandDouble()) == 2
  parameters["AllocationSamplingMultiplier"] = "2";

  // Set browser-specific sampling frequency:
  // 2000 * (1 ** RandDouble()) == 2000
  parameters["BrowserAllocationSamplingMultiplier"] = "2000";

  // Set renderer-specific sampling frequency:
  // 300 * (1 ** RandDouble()) == 300
  parameters["RendererAllocationSamplingMultiplier"] = "300";

  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(kTestFeature3, parameters);

  const auto generic_settings = GetAllocatorSettingsImpl(
      kTestFeature3, false, "invalid process type to force generic params");
  EXPECT_TRUE(generic_settings.has_value());
  EXPECT_EQ(generic_settings->sampling_frequency, 2ul);

  const auto renderer_settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, "renderer");
  EXPECT_TRUE(renderer_settings.has_value());
  EXPECT_EQ(renderer_settings->sampling_frequency, 300ul);

  // The empty `process_type` string denotes the browser process.
  const auto browser_settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, "");
  EXPECT_TRUE(browser_settings.has_value());
  EXPECT_EQ(browser_settings->sampling_frequency, 2000ul);
}

TEST(GwpAsanTest, GetProcessSpecificAllocationSamplingRange) {
  std::map<std::string, std::string> parameters;

  // This parameter is never overridden, hence common to all processes.
  parameters["AllocationSamplingMultiplier"] = "1";

  // The default sampling frequency, then, is
  // 1 * (1048576 ** RandDouble()) >= 1
  // ...and will _most likely_ not equal 1.
  parameters["AllocationSamplingRange"] = "1048576";

  // Set browser-specific sampling frequency:
  // 1 * (1 ** RandDouble()) == 1
  parameters["BrowserAllocationSamplingRange"] = "1";

  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeatureWithParameters(kTestFeature3, parameters);

  const auto generic_settings = GetAllocatorSettingsImpl(
      kTestFeature3, false, "invalid process type to force generic params");
  EXPECT_TRUE(generic_settings.has_value());
  EXPECT_GE(generic_settings->sampling_frequency, 1ul);
  EXPECT_LE(generic_settings->sampling_frequency, 1ul << 20);

  // The empty `process_type` string denotes the browser process.
  const auto browser_settings =
      GetAllocatorSettingsImpl(kTestFeature3, false, "");
  EXPECT_TRUE(browser_settings.has_value());
  EXPECT_EQ(browser_settings->sampling_frequency, 1ul);
}

}  // namespace internal
}  // namespace gwp_asan
