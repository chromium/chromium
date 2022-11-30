// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_parameters.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/metrics/call_stack_profile_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {

namespace {

using ::testing::AllOf;
using ::testing::Field;

// Can't define operator== because gmock has a conflicting operator== in an
// internal namespace.
auto MatchesParameters(const HeapProfilerParameters& expected) {
  return AllOf(
      Field("is_supported", &HeapProfilerParameters::is_supported,
            expected.is_supported),
      Field("stable_probability", &HeapProfilerParameters::stable_probability,
            expected.stable_probability),
      Field("nonstable_probability",
            &HeapProfilerParameters::nonstable_probability,
            expected.nonstable_probability),
      Field("sampling_rate_bytes", &HeapProfilerParameters::sampling_rate_bytes,
            expected.sampling_rate_bytes),
      Field("collection_interval", &HeapProfilerParameters::collection_interval,
            expected.collection_interval));
}

TEST(HeapProfilerParametersTest, ParseEmptyParameters) {
  constexpr char kJSONParams[] = R"({})";
  HeapProfilerParameters params;
  EXPECT_TRUE(params.UpdateFromJSON(kJSONParams));
  EXPECT_THAT(params, MatchesParameters({}));
}

TEST(HeapProfilerParametersTest, ParseParameters) {
  constexpr char kJSONParams[] = R"({
    "is-supported": true,
    "stable-probability": 0.1,
    // Comments should be allowed.
    // Double parameters should convert from integers.
    "nonstable-probability": 1,
    "sampling-rate-bytes": 1000,
    "collection-interval-minutes": 30,
  })";
  HeapProfilerParameters params;
  EXPECT_TRUE(params.UpdateFromJSON(kJSONParams));
  EXPECT_THAT(params, MatchesParameters({
                          .is_supported = true,
                          .stable_probability = 0.1,
                          .nonstable_probability = 1.0,
                          .sampling_rate_bytes = 1000,
                          .collection_interval = base::Minutes(30),
                      }));
}

TEST(HeapProfilerParametersTest, ParsePartialParameters) {
  constexpr char kJSONParams[] = R"({
    "is-supported": false,
    "stable-probability": 0.5,
    "collection-interval-minutes": 60,
  })";
  // Only the parameters that are included in the JSON should be overwritten.
  HeapProfilerParameters params{
      .is_supported = true,
      .stable_probability = 0.1,
      .nonstable_probability = 0.2,
      .sampling_rate_bytes = 1000,
      .collection_interval = base::Minutes(30),
  };
  EXPECT_TRUE(params.UpdateFromJSON(kJSONParams));
  EXPECT_THAT(params, MatchesParameters({
                          .is_supported = false,
                          .stable_probability = 0.5,
                          .nonstable_probability = 0.2,
                          .sampling_rate_bytes = 1000,
                          .collection_interval = base::Minutes(60),
                      }));
}

TEST(HeapProfilerParametersTest, ParseInvalidParameters) {
  constexpr char kJSONParams[] = R"({
    "collection-interval-minutes": -1,
  })";
  HeapProfilerParameters params;
  EXPECT_FALSE(params.UpdateFromJSON(kJSONParams));
  EXPECT_FALSE(params.is_supported);
}

TEST(HeapProfilerParametersTest, ApplyParameters) {
  constexpr char kBrowserParams[] = R"({
    "sampling-rate-bytes": 1001,
  })";
  constexpr char kGPUParams[] = R"({
    "is-supported": true,
    "sampling-rate-bytes": 1002,
    "collection-interval-minutes": 60,
  })";
  constexpr char kRendererParams[] = R"({
    "is-supported": false,
    "sampling-rate-bytes": 1003,
  })";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kHeapProfilerReporting,
      {
          // Default parameters.
          {"supported-processes", "renderer;utility"},
          {"stable-probability", "0.1"},
          {"nonstable-probability", "0.2"},
          {"sampling-rate", "1000"},
          {"heap-profiler-collection-interval-minutes", "15"},

          // Process-specific overrides.
          {"browser-process-params", kBrowserParams},
          {"gpu-process-params", kGPUParams},
          {"renderer-process-params", kRendererParams},
          {"utility-process-params", "{}"},
      });

  EXPECT_THAT(GetDefaultHeapProfilerParameters(),
              MatchesParameters({
                  .is_supported = false,
                  .stable_probability = 0.1,
                  .nonstable_probability = 0.2,
                  .sampling_rate_bytes = 1000,
                  .collection_interval = base::Minutes(15),
              }));

  using Process = metrics::CallStackProfileParams::Process;
  EXPECT_THAT(GetHeapProfilerParametersForProcess(Process::kBrowser),
              MatchesParameters({
                  // Not in "supported-processes" and not overridden.
                  .is_supported = false,
                  .stable_probability = 0.1,
                  .nonstable_probability = 0.2,
                  .sampling_rate_bytes = 1001,
                  .collection_interval = base::Minutes(15),
              }));

  EXPECT_THAT(GetHeapProfilerParametersForProcess(Process::kGpu),
              MatchesParameters({
                  // Not in "supported-processes" but overridden to true.
                  .is_supported = true,
                  .stable_probability = 0.1,
                  .nonstable_probability = 0.2,
                  .sampling_rate_bytes = 1002,
                  .collection_interval = base::Minutes(60),
              }));

  EXPECT_THAT(GetHeapProfilerParametersForProcess(Process::kRenderer),
              MatchesParameters({
                  // In "supported-processes", but overridden to false.
                  .is_supported = false,
                  .stable_probability = 0.1,
                  .nonstable_probability = 0.2,
                  .sampling_rate_bytes = 1003,
                  .collection_interval = base::Minutes(15),
              }));

  EXPECT_THAT(GetHeapProfilerParametersForProcess(Process::kUtility),
              MatchesParameters({
                  // In "supported-processes".
                  .is_supported = true,
                  .stable_probability = 0.1,
                  .nonstable_probability = 0.2,
                  .sampling_rate_bytes = 1000,
                  .collection_interval = base::Minutes(15),
              }));

  EXPECT_THAT(GetHeapProfilerParametersForProcess(Process::kNetworkService),
              MatchesParameters({
                  // Not in "supported-processes" and not overridden.
                  .is_supported = false,
                  .stable_probability = 0.1,
                  .nonstable_probability = 0.2,
                  .sampling_rate_bytes = 1000,
                  .collection_interval = base::Minutes(15),
              }));
}

}  // namespace

}  // namespace heap_profiling
