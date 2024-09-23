// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/timer/lap_timer.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_type.mojom.h"
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

struct ScopesConfig {
  uint32_t attribution_scope_limit;
  uint32_t max_event_states;
};

using ScopedCollapsibleTriggerConfig =
    std::tuple</*collapse=*/bool, TriggerConfig, std::optional<ScopesConfig>>;

std::string StoryName(const ScopedCollapsibleTriggerConfig& p) {
  std::stringstream name;

  const TriggerConfig& tc = std::get<1>(p);
  name << tc.max_reports << "r_"  //
       << tc.num_windows << "w_"  //
       << tc.trigger_data_cardinality << "t";

  if (const std::optional<ScopesConfig>& scopes = std::get<2>(p)) {
    name << "_" << scopes->attribution_scope_limit << "s_"  //
         << scopes->max_event_states << "m_scoped";
  }

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

constexpr std::optional<ScopesConfig> kScopeConfigs[] = {
    // null scopes
    std::nullopt,
    // simple scopes
    ScopesConfig{
        .attribution_scope_limit = 3,
        .max_event_states = 3,
    },
    ScopesConfig{
        .attribution_scope_limit = 5,
        .max_event_states = std::numeric_limits<uint32_t>::max(),
    },
    ScopesConfig{
        .attribution_scope_limit = 10,
        .max_event_states = std::numeric_limits<uint32_t>::max(),
    },
    ScopesConfig{
        .attribution_scope_limit = 20,
        .max_event_states = std::numeric_limits<uint32_t>::max(),
    },
};

class PrivacyMathPerfTest
    : public testing::Test,
      public testing::WithParamInterface<ScopedCollapsibleTriggerConfig> {
 protected:
  template <typename Func>
  void Run(const std::string& metric_basename, Func&& func) const {
    const auto& [collapse, tc, sc] = GetParam();
    const TriggerSpecs specs = SpecsFromWindowList(
        std::vector<int>(/*count=*/tc.trigger_data_cardinality,
                         /*value=*/tc.num_windows),
        collapse, MaxEventLevelReports(tc.max_reports));
    std::optional<AttributionScopesData> scopes;
    if (sc.has_value()) {
      scopes = AttributionScopesData::Create(AttributionScopesSet({"1"}),
                                             sc->attribution_scope_limit,
                                             sc->max_event_states);
      ASSERT_TRUE(scopes);
    }

    base::LapTimer timer;
    do {
      auto result = func(specs, scopes);
      ::benchmark::DoNotOptimize(result);
      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter(metric_basename,
                                           StoryName(GetParam()));
    reporter.RegisterImportantMetric(".wall_time", "us");
    reporter.AddResult(".wall_time", timer.TimePerLap());
  }
};

TEST_P(PrivacyMathPerfTest, NumStates) {
  if (std::get<2>(GetParam())) {
    GTEST_SKIP();
  }
  Run("AttributionReporting.NumStates",
      [](const TriggerSpecs& specs,
         const std::optional<AttributionScopesData>& scopes) {
        return GetNumStates(specs);
      });
}

TEST_P(PrivacyMathPerfTest, RandomizedResponse) {
  constexpr PrivacyMathConfig kConfig{
      .max_channel_capacity_navigation =
          std::numeric_limits<double>::infinity(),
      .max_channel_capacity_event = std::numeric_limits<double>::infinity(),
  };

  Run("AttributionReporting.RandomizedResponse",
      [&](const TriggerSpecs& specs,
          const std::optional<AttributionScopesData>& scopes) {
        return DoRandomizedResponse(
            specs,
            /*epsilon=*/0,
            /*source_type=*/mojom::SourceType::kNavigation, scopes, kConfig);
      });
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacyMathPerfTest,
    testing::Combine(/*collapse=*/testing::Bool(),
                     testing::ValuesIn(kTriggerConfigs),
                     testing::ValuesIn(kScopeConfigs)),
    [](const testing::TestParamInfo<ScopedCollapsibleTriggerConfig>& info) {
      return StoryName(info.param);
    });

}  // namespace
}  // namespace attribution_reporting
