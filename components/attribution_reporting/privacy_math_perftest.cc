// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/privacy_math.h"

#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
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

struct TriggerConfig {
  int max_reports;
  int num_windows;
  int trigger_data_cardinality;
};

using CollapsibleTriggerConfig = std::tuple</*collapse=*/bool, TriggerConfig>;

std::string StoryName(const CollapsibleTriggerConfig& p) {
  std::stringstream name;

  const TriggerConfig& tc = std::get<1>(p);
  name << tc.max_reports << "r_"  //
       << tc.num_windows << "w_"  //
       << tc.trigger_data_cardinality << "t";

  if (std::get<0>(p)) {
    name << "_collapsed";
  }
  return std::move(name).str();
}

constexpr TriggerConfig kTriggerConfigs[] = {
    // default navigation source
    {
        .max_reports = 3,
        .num_windows = 3,
        .trigger_data_cardinality = 8,
    },
    // default event source
    {
        .max_reports = 1,
        .num_windows = 1,
        .trigger_data_cardinality = 2,
    },
    {
        .max_reports = 20,
        .num_windows = 5,
        .trigger_data_cardinality = 8,
    },
    {
        .max_reports = 20,
        .num_windows = 5,
        .trigger_data_cardinality = 32,
    },
};

class PrivacyMathPerfTest
    : public testing::Test,
      public testing::WithParamInterface<CollapsibleTriggerConfig> {
 protected:
  template <typename Func>
  void Run(const std::string& metric_basename, Func&& func) const {
    const auto& [collapse, tc] = GetParam();
    const TriggerSpecs specs = SpecsFromWindowList(
        std::vector<int>(/*count=*/tc.trigger_data_cardinality,
                         /*value=*/tc.num_windows),
        collapse, MaxEventLevelReports(tc.max_reports));

    base::LapTimer timer;
    do {
      auto result = func(specs);
      ::benchmark::DoNotOptimize(result);
      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter(metric_basename,
                                           StoryName(GetParam()));
    reporter.RegisterImportantMetric(".wall_time", "ms");
    reporter.AddResult(".wall_time", timer.TimePerLap());
  }
};

TEST_P(PrivacyMathPerfTest, NumStates) {
  Run("AttributionReporting.NumStates", &GetNumStates);
}

TEST_P(PrivacyMathPerfTest, RandomizedResponse) {
  Run("AttributionReporting.RandomizedResponse", [](const TriggerSpecs& specs) {
    return DoRandomizedResponse(
        specs,
        /*epsilon=*/0,
        /*max_channel_capacity=*/std::numeric_limits<double>::infinity());
  });
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacyMathPerfTest,
    testing::Combine(/*collapse=*/testing::Bool(),
                     testing::ValuesIn(kTriggerConfigs)),
    [](const testing::TestParamInfo<CollapsibleTriggerConfig>& info) {
      return StoryName(info.param);
    });

}  // namespace
}  // namespace attribution_reporting
