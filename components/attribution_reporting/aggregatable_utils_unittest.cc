// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_utils.h"

#include <math.h>

#include <optional>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;

using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(AggregatableUtilsTest, RoundDownToWholeDaySinceUnixEpoch) {
  const struct {
    const char* desc;
    base::TimeDelta time;
    base::TimeDelta expected;
  } kTestCases[] = {
      {
          .desc = "whole-day",
          .time = base::Days(14288),
          .expected = base::Days(14288),
      },
      {
          .desc = "whole-day-plus-one-second",
          .time = base::Days(14288) + base::Seconds(1),
          .expected = base::Days(14288),
      },
      {
          .desc = "half-day-minus-one-second",
          .time = base::Days(14288) + base::Hours(12) - base::Seconds(1),
          .expected = base::Days(14288),
      },
      {
          .desc = "whole-day-minus-one-second",
          .time = base::Days(14289) - base::Seconds(1),
          .expected = base::Days(14288),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    EXPECT_EQ(RoundDownToWholeDaySinceUnixEpoch(base::Time::UnixEpoch() +
                                                test_case.time),
              base::Time::UnixEpoch() + test_case.expected);
  }
}

TEST(AggregatableUtilsTest,
     GetNullAggregatableReports_IncludeSourceRegistrationTime) {
  auto config =
      *AggregatableTriggerConfig::Create(SourceRegistrationTimeConfig::kInclude,
                                         /*trigger_context_id=*/std::nullopt);
  base::Time now = base::Time::Now();

  const auto always_true = [](int, SourceRegistrationTimeConfig) {
    return true;
  };

  const auto always_false = [](int, SourceRegistrationTimeConfig) {
    return false;
  };

  const auto selective = [](int day, SourceRegistrationTimeConfig) {
    return day == 0 || day == 5;
  };

  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_true),
              SizeIs(31));
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_false),
              IsEmpty());
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, selective),
              SizeIs(2));

  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_true),
      SizeIs(30));
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_false),
      IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, selective),
      SizeIs(1));
}

TEST(AggregatableUtilsTest,
     GetNullAggregatableReports_ExcludeSourceRegistrationTime) {
  auto config =
      *AggregatableTriggerConfig::Create(SourceRegistrationTimeConfig::kExclude,
                                         /*trigger_context_id=*/std::nullopt);
  base::Time now = base::Time::Now();

  const auto always_true = [](int, SourceRegistrationTimeConfig) {
    return true;
  };

  const auto always_false = [](int, SourceRegistrationTimeConfig) {
    return false;
  };

  const auto selective = [](int day, SourceRegistrationTimeConfig) {
    return day == 0 || day == 5;
  };

  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_true),
              SizeIs(1));
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_false),
              IsEmpty());
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, selective),
              SizeIs(1));

  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_true),
      IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_false),
      IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, selective),
      IsEmpty());
}

TEST(AggregatableUtilsTest, GetNullAggregatableReports_TriggerContextId) {
  auto config =
      *AggregatableTriggerConfig::Create(SourceRegistrationTimeConfig::kExclude,
                                         /*trigger_context_id=*/"");
  base::Time now = base::Time::Now();

  const auto always_true = [](int, SourceRegistrationTimeConfig) {
    return true;
  };

  const auto always_false = [](int, SourceRegistrationTimeConfig) {
    return false;
  };

  const auto selective = [](int day, SourceRegistrationTimeConfig) {
    return day == 0 || day == 5;
  };

  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_true),
              SizeIs(1));
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_false),
              SizeIs(1));
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, selective),
              SizeIs(1));

  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_true),
      IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_false),
      IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, selective),
      IsEmpty());
}

double GetNullReportRate(SourceRegistrationTimeConfig config) {
  switch (config) {
    case SourceRegistrationTimeConfig::kInclude:
      return 0.008;
    case SourceRegistrationTimeConfig::kExclude:
      return 0.05;
  }
}

int GetNumLookbackDays(SourceRegistrationTimeConfig config) {
  switch (config) {
    case SourceRegistrationTimeConfig::kInclude:
      return 31;
    case SourceRegistrationTimeConfig::kExclude:
      return 1;
  }
}

bool MaybeGenerateNullReport(int lookback_day,
                             SourceRegistrationTimeConfig config) {
  return GenerateWithRate(GetNullReportRate(config));
}

struct NullReportsTestCase {
  const char* desc;
  AggregatableTriggerConfig config;
};

const NullReportsTestCase kNullReportsTestCases[] = {
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

class AggregatableUtilsNullReportsTest
    : public testing::Test,
      public testing::WithParamInterface<NullReportsTestCase> {};

TEST_P(AggregatableUtilsNullReportsTest, ExpectedDistribution) {
  const auto& test_case = GetParam();

  const base::Time trigger_time = base::Time::Now();
  const std::optional<base::Time> attributed_source_time;
  const int num_samples = 100'000;

  int actual_count = 0;
  for (int i = 0; i < num_samples; i++) {
    auto result = GetNullAggregatableReports(test_case.config, trigger_time,
                                             attributed_source_time,
                                             &MaybeGenerateNullReport);
    actual_count += result.size();
  }

  const auto source_registration_time_config =
      test_case.config.source_registration_time_config();

  int num_total_samples =
      num_samples * GetNumLookbackDays(source_registration_time_config);
  double rate = GetNullReportRate(source_registration_time_config);

  int expected_count = num_total_samples * rate;

  // The variance of the binomial distribution is np(1-p), and critical value z
  // = 2.575 is used for a test of significance at 0.01 level. The check will
  // fail with probability 0.01.
  EXPECT_NEAR(actual_count, expected_count,
              2.575 * sqrt(num_total_samples * rate * (1. - rate)));
}

INSTANTIATE_TEST_SUITE_P(,
                         AggregatableUtilsNullReportsTest,
                         testing::ValuesIn(kNullReportsTestCases));

}  // namespace
}  // namespace attribution_reporting
