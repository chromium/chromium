// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/timer/lap_timer.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {
namespace {

struct NumStatesTestCase {
  std::string story_name;
  MaxEventLevelReports max_reports;
  std::vector<int> windows_per_type;
  absl::uint128 expected_num_states;
};

const NumStatesTestCase kNumStatesTestCases[] = {
    {"default_nav", MaxEventLevelReports(3), {3, 3, 3, 3, 3, 3, 3, 3}, 2925},
    {"default_event", MaxEventLevelReports(1), {1, 1}, 3},

    // r = max event level reports
    // w = num windows
    // t = trigger data types
    {"(20r,5w,8t)",
     MaxEventLevelReports(20),
     {5, 5, 5, 5, 5, 5, 5, 5},
     4191844505805495},

    {"(20r,5w,32t)",
     MaxEventLevelReports(20),
     {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
     absl::MakeUint128(/*high=*/9494472u, /*low=*/10758590974061625903u)},
};

class PrivacyMathPerfTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, NumStatesTestCase>> {
};

TEST_P(PrivacyMathPerfTest, NumStates) {
  const auto [collapse, test_case] = GetParam();
  base::LapTimer timer;
  const auto specs = SpecsFromWindowList(test_case.windows_per_type, collapse);
  bool valid = true;
  do {
    // Do a trivial check to ensure the GetNumStates call is not optimized by
    // the compiler.
    valid &= GetNumStates(specs, test_case.max_reports) ==
             test_case.expected_num_states;
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());
  CHECK(valid);

  std::string story = test_case.story_name + (collapse ? "(collapsed)" : "");
  perf_test::PerfResultReporter reporter("AttributionReporting.NumStates",
                                         story);
  reporter.RegisterImportantMetric(".wall_time", "ms");
  reporter.AddResult(".wall_time", 1e6 / timer.LapsPerSecond());
}

TEST_P(PrivacyMathPerfTest, RandomizedResponse) {
  const auto [collapse, test_case] = GetParam();
  base::LapTimer timer;
  const auto specs = SpecsFromWindowList(test_case.windows_per_type, collapse);
  bool valid_rates = true;
  do {
    auto response_data = DoRandomizedResponse(specs, test_case.max_reports,
                                              /*epsilon=*/0);
    // Do a trivial check to ensure the call is not optimized by the compiler.
    valid_rates &= response_data.rate() >= 0;
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());
  CHECK(valid_rates);

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
