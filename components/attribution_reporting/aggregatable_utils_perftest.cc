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
};

const TestCase kTestCases[] = {
    {
        "include_no_attributed_source_time",
        *AggregatableTriggerConfig::Create(
            SourceRegistrationTimeConfig::kInclude,
            /*trigger_context_id=*/std::nullopt),
    },
    {
        "exclude_no_attributed_source_time_no_trigger_context_id",
        *AggregatableTriggerConfig::Create(
            SourceRegistrationTimeConfig::kExclude,
            /*trigger_context_id=*/std::nullopt),
    },
};

class AggregatableUtilsPerfTest : public testing::Test,
                                  public testing::WithParamInterface<TestCase> {
};

// TODO(apaseltiner): This duplicates logic in
// `content::AttributionStorageDelegateImpl`. We should refactor in order to
// avoid this and make it clear that the production behavior is being
// benchmarked properly.
bool MaybeGenerateNullReport(int lookback_day,
                             SourceRegistrationTimeConfig config) {
  switch (config) {
    case SourceRegistrationTimeConfig::kInclude:
      return GenerateWithRate(0.008);
    case SourceRegistrationTimeConfig::kExclude:
      return GenerateWithRate(0.05);
  }
}

TEST_P(AggregatableUtilsPerfTest, GetNullAggregatableReports) {
  const auto& test_case = GetParam();

  const base::Time trigger_time = base::Time::Now();
  const std::optional<base::Time> attributed_source_time;

  base::LapTimer timer;
  do {
    auto result = GetNullAggregatableReports(test_case.config, trigger_time,
                                             attributed_source_time,
                                             &MaybeGenerateNullReport);

    ::benchmark::DoNotOptimize(result);

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter(
      "AttributionReporting.GetNullAggregatableReports", test_case.story_name);
  reporter.RegisterImportantMetric(".wall_time", "ms");
  reporter.AddResult(".wall_time", 1e6 / timer.LapsPerSecond());
}

INSTANTIATE_TEST_SUITE_P(,
                         AggregatableUtilsPerfTest,
                         testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace attribution_reporting
