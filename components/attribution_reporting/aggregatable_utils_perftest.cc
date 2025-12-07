// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;

struct TestCase {
  std::string story_name;
  AggregatableTriggerConfig config;
  double rate;
};

const TestCase kTestCases[] = {
    {
        "include_no_attributed_source_time",
        *AggregatableTriggerConfig::Create(
            SourceRegistrationTimeConfig::kInclude,
            /*trigger_context_id=*/std::nullopt,
            AggregatableFilteringIdsMaxBytes()),
        0.008,
    },
    {
        "exclude_no_attributed_source_time_no_trigger_context_id",
        *AggregatableTriggerConfig::Create(
            SourceRegistrationTimeConfig::kExclude,
            /*trigger_context_id=*/std::nullopt,
            AggregatableFilteringIdsMaxBytes()),
        0.05,
    },
};

class AggregatableUtilsPerfTest : public testing::Test,
                                  public testing::WithParamInterface<TestCase> {
};

TEST_P(AggregatableUtilsPerfTest, GetNullAggregatableReports) {
  const TestCase& test_case = GetParam();

  const base::Time trigger_time = base::Time::Now();
  const std::optional<base::Time> attributed_source_time;

  // TODO(apaseltiner): This duplicates logic in
  // `content::AttributionStorageDelegateImpl`. We should refactor in order to
  // avoid this and make it clear that the production behavior is being
  // benchmarked properly.
  const auto generate = [&](int lookback_day) {
    return GenerateWithRate(test_case.rate);
  };

  base::LapTimer timer;
  do {
    auto result = GetNullAggregatableReports(test_case.config, trigger_time,
                                             attributed_source_time, generate);

    ::benchmark::DoNotOptimize(result);

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter(
      "AttributionReporting.GetNullAggregatableReports", test_case.story_name);
  reporter.RegisterImportantMetric(".wall_time", "ms");
  reporter.AddResult(".wall_time", timer.TimePerLap());
}

INSTANTIATE_TEST_SUITE_P(,
                         AggregatableUtilsPerfTest,
                         testing::ValuesIn(kTestCases),
                         [](const testing::TestParamInfo<TestCase>& info) {
                           return info.param.story_name;
                         });

}  // namespace
}  // namespace attribution_reporting
