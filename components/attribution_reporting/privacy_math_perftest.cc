// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/privacy_math.h"

#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "base/timer/lap_timer.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace attribution_reporting {
namespace {

struct NumStatesTestCase {
  std::string story_name;
  MaxEventLevelReports max_reports;
  std::vector<int> windows_per_type;
};

const NumStatesTestCase kNumStatesTestCases[] = {
    {"default_nav", MaxEventLevelReports(3),
     std::vector<int>(/*count=*/8, /*value=*/3)},

    {"default_event", MaxEventLevelReports(1),
     std::vector<int>(/*count=*/2, /*value=*/1)},

    // r = max event level reports
    // w = num windows
    // t = trigger data types
    {"(20r,5w,8t)", MaxEventLevelReports(20),
     std::vector<int>(/*count=*/8, /*value=*/5)},

    {"(20r,5w,32t)", MaxEventLevelReports(20),
     std::vector<int>(/*count=*/32, /*value=*/5)},
};

class PrivacyMathPerfTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, NumStatesTestCase>> {
};

TEST_P(PrivacyMathPerfTest, NumStates) {
  const auto [collapse, test_case] = GetParam();
  const auto specs = SpecsFromWindowList(test_case.windows_per_type, collapse,
                                         test_case.max_reports);

  base::LapTimer timer;
  do {
    auto result = GetNumStates(specs);

    ::benchmark::DoNotOptimize(result);

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  std::string story = test_case.story_name + (collapse ? "(collapsed)" : "");
  perf_test::PerfResultReporter reporter("AttributionReporting.NumStates",
                                         story);
  reporter.RegisterImportantMetric(".wall_time", "ms");
  reporter.AddResult(".wall_time", 1e6 / timer.LapsPerSecond());
}

TEST_P(PrivacyMathPerfTest, RandomizedResponse) {
  const auto [collapse, test_case] = GetParam();
  const auto specs = SpecsFromWindowList(test_case.windows_per_type, collapse,
                                         test_case.max_reports);

  base::LapTimer timer;
  do {
    auto result = DoRandomizedResponse(
        specs,
        /*epsilon=*/0,
        /*max_channel_capacity=*/std::numeric_limits<double>::infinity());

    ::benchmark::DoNotOptimize(result);

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  std::string story = test_case.story_name + (collapse ? " (collapsed)" : "");
  perf_test::PerfResultReporter reporter(
      "AttributionReporting.RandomizedResponse", story);
  reporter.RegisterImportantMetric(".wall_time", "ms");
  reporter.AddResult(".wall_time", (1e6 / timer.LapsPerSecond()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacyMathPerfTest,
    testing::Combine(testing::Bool(), testing::ValuesIn(kNumStatesTestCases)));

}  // namespace
}  // namespace attribution_reporting
