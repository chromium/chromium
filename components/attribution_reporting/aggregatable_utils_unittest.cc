// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_utils.h"

#include <math.h>

#include <optional>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
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
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

const AggregatableFilteringIdsMaxBytes kFilteringIdMaxBytes;

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
  auto config = *AggregatableTriggerConfig::Create(
      SourceRegistrationTimeConfig::kInclude,
      /*trigger_context_id=*/std::nullopt, kFilteringIdMaxBytes);
  base::Time now = base::Time::Now();

  const auto always_true = [](int) { return true; };

  const auto always_false = [](int) { return false; };

  const auto selective = [](int day) { return day == 0 || day == 5; };

  std::vector<NullAggregatableReport> expected_all;
  expected_all.reserve(31);
  for (int i = 0; i < 31; i++) {
    expected_all.emplace_back(now - base::Days(i));
  }
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_true),
              UnorderedElementsAreArray(expected_all));
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_false),
              IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/std::nullopt,
                                 selective),
      UnorderedElementsAre(NullAggregatableReport(now),
                           NullAggregatableReport(now - base::Days(5))));

  expected_all.erase(expected_all.begin());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_true),
      UnorderedElementsAreArray(expected_all));
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, always_false),
      IsEmpty());
  EXPECT_THAT(
      GetNullAggregatableReports(config,
                                 /*trigger_time=*/now,
                                 /*attributed_source_time=*/now, selective),
      UnorderedElementsAre(NullAggregatableReport(now - base::Days(5))));
}

TEST(AggregatableUtilsTest,
     GetNullAggregatableReports_ExcludeSourceRegistrationTime) {
  auto config = *AggregatableTriggerConfig::Create(
      SourceRegistrationTimeConfig::kExclude,
      /*trigger_context_id=*/std::nullopt, kFilteringIdMaxBytes);
  base::Time now = base::Time::Now();

  const auto always_true = [](int) { return true; };

  const auto always_false = [](int) { return false; };

  const auto selective = [](int day) { return day == 0 || day == 5; };

  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_true),
              UnorderedElementsAre(NullAggregatableReport(now)));
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, always_false),
              IsEmpty());
  EXPECT_THAT(GetNullAggregatableReports(
                  config,
                  /*trigger_time=*/now,
                  /*attributed_source_time=*/std::nullopt, selective),
              UnorderedElementsAre(NullAggregatableReport(now)));

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

TEST(AggregatableUtilsTest, GetNullAggregatableReports_RoundedTime) {
  auto config = *AggregatableTriggerConfig::Create(
      SourceRegistrationTimeConfig::kInclude,
      /*trigger_context_id=*/std::nullopt, kFilteringIdMaxBytes);
  const auto generate_func = [](int day) {
    return day == 3 || day == 4 || day == 5;
  };

  const base::Time whole_day = base::Time::UnixEpoch() + base::Days(14288);

  const std::vector<base::TimeDelta> time_deltas = {
      base::TimeDelta(), base::Seconds(1), base::Hours(12),
      base::Days(1) - base::Seconds(1)};

  for (base::TimeDelta source_time_delta : time_deltas) {
    SCOPED_TRACE(source_time_delta);
    for (base::TimeDelta trigger_time_delta : time_deltas) {
      SCOPED_TRACE(trigger_time_delta);

      // Rounded down to `whole_day` + base::Days(4).
      base::Time trigger_time = whole_day + base::Days(4) + trigger_time_delta;

      EXPECT_THAT(GetNullAggregatableReports(config, trigger_time,
                                             // Rounded down to `whole_day`.
                                             whole_day + source_time_delta,
                                             generate_func),
                  UnorderedElementsAre(
                      NullAggregatableReport(trigger_time - base::Days(3)),
                      NullAggregatableReport(trigger_time - base::Days(5))));
    }
  }
}

TEST(AggregatableUtilsTest, GetNullAggregatableReportsUnconditionally) {
  const struct {
    const char* description;
    AggregatableTriggerConfig config;
  } kTestCases[] = {
      {
          .description = "Trigger context id",
          .config = *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude,
              /*trigger_context_id=*/"", kFilteringIdMaxBytes),
      },
      {
          .description = "Non-default filtering id max bytes",
          .config = *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude,
              /*trigger_context_id=*/std::nullopt,
              *AggregatableFilteringIdsMaxBytes::Create(3)),
      },
  };
  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    base::Time now = base::Time::Now();

    const AggregatableTriggerConfig& config = test_case.config;

    const auto always_true = [](int) { return true; };

    const auto always_false = [](int) { return false; };

    const auto selective = [](int day) { return day == 0 || day == 5; };

    EXPECT_THAT(GetNullAggregatableReports(
                    config,
                    /*trigger_time=*/now,
                    /*attributed_source_time=*/std::nullopt, always_true),
                UnorderedElementsAre(NullAggregatableReport(now)));
    EXPECT_THAT(GetNullAggregatableReports(
                    config,
                    /*trigger_time=*/now,
                    /*attributed_source_time=*/std::nullopt, always_false),
                UnorderedElementsAre(NullAggregatableReport(now)));
    EXPECT_THAT(GetNullAggregatableReports(
                    config,
                    /*trigger_time=*/now,
                    /*attributed_source_time=*/std::nullopt, selective),
                UnorderedElementsAre(NullAggregatableReport(now)));

    EXPECT_THAT(
        GetNullAggregatableReports(config,
                                   /*trigger_time=*/now,
                                   /*attributed_source_time=*/now, always_true),
        IsEmpty());
    EXPECT_THAT(GetNullAggregatableReports(config,
                                           /*trigger_time=*/now,
                                           /*attributed_source_time=*/now,
                                           always_false),
                IsEmpty());
    EXPECT_THAT(
        GetNullAggregatableReports(config,
                                   /*trigger_time=*/now,
                                   /*attributed_source_time=*/now, selective),
        IsEmpty());
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

struct NullReportsTestCase {
  const char* desc;
  AggregatableTriggerConfig config;
  double rate;
};

const NullReportsTestCase kNullReportsTestCases[] = {
    {
        "include_no_attributed_source_time",
        *AggregatableTriggerConfig::Create(
            SourceRegistrationTimeConfig::kInclude,
            /*trigger_context_id=*/std::nullopt,
            kFilteringIdMaxBytes),
        0.008,
    },
    {
        "exclude_no_attributed_source_time_no_trigger_context_id",
        *AggregatableTriggerConfig::Create(
            SourceRegistrationTimeConfig::kExclude,
            /*trigger_context_id=*/std::nullopt,
            kFilteringIdMaxBytes),
        0.05,
    },
};

class AggregatableUtilsNullReportsTest
    : public testing::Test,
      public testing::WithParamInterface<NullReportsTestCase> {};

TEST_P(AggregatableUtilsNullReportsTest, ExpectedDistribution) {
  const auto& test_case = GetParam();

  const auto generate = [&](int lookback_day) {
    return GenerateWithRate(test_case.rate);
  };

  const base::Time trigger_time = base::Time::Now();
  const std::optional<base::Time> attributed_source_time;
  const int num_samples = 100'000;

  int actual_count = 0;
  for (int i = 0; i < num_samples; i++) {
    auto result = GetNullAggregatableReports(test_case.config, trigger_time,
                                             attributed_source_time, generate);
    actual_count += result.size();
  }

  const auto source_registration_time_config =
      test_case.config.source_registration_time_config();

  int num_total_samples =
      num_samples * GetNumLookbackDays(source_registration_time_config);

  int expected_count = num_total_samples * test_case.rate;

  // The variance of the binomial distribution is np(1-p), and critical value z
  // = 2.575 is used for a test of significance at 0.01 level. The check will
  // fail with probability 0.01.
  EXPECT_NEAR(
      actual_count, expected_count,
      2.575 * sqrt(num_total_samples * test_case.rate * (1. - test_case.rate)));
}

INSTANTIATE_TEST_SUITE_P(,
                         AggregatableUtilsNullReportsTest,
                         testing::ValuesIn(kNullReportsTestCases));

}  // namespace
}  // namespace attribution_reporting
