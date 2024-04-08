// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_utils.h"

#include <optional>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

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
  auto config = *AggregatableTriggerConfig::Create(
      mojom::SourceRegistrationTimeConfig::kInclude,
      /*trigger_context_id=*/std::nullopt);
  base::Time now = base::Time::Now();

  const auto always_true = [](int, mojom::SourceRegistrationTimeConfig) {
    return true;
  };

  const auto always_false = [](int, mojom::SourceRegistrationTimeConfig) {
    return false;
  };

  const auto selective = [](int day, mojom::SourceRegistrationTimeConfig) {
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
  auto config = *AggregatableTriggerConfig::Create(
      mojom::SourceRegistrationTimeConfig::kExclude,
      /*trigger_context_id=*/std::nullopt);
  base::Time now = base::Time::Now();

  const auto always_true = [](int, mojom::SourceRegistrationTimeConfig) {
    return true;
  };

  const auto always_false = [](int, mojom::SourceRegistrationTimeConfig) {
    return false;
  };

  const auto selective = [](int day, mojom::SourceRegistrationTimeConfig) {
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
  auto config = *AggregatableTriggerConfig::Create(
      mojom::SourceRegistrationTimeConfig::kExclude,
      /*trigger_context_id=*/"");
  base::Time now = base::Time::Now();

  const auto always_true = [](int, mojom::SourceRegistrationTimeConfig) {
    return true;
  };

  const auto always_false = [](int, mojom::SourceRegistrationTimeConfig) {
    return false;
  };

  const auto selective = [](int day, mojom::SourceRegistrationTimeConfig) {
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

}  // namespace
}  // namespace attribution_reporting
