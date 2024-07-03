// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/summary_buckets.h"

#include <stdint.h>

#include <utility>

#include "base/containers/flat_set.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/summary_operator.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SummaryOperator;
using ::base::test::ErrorIs;
using ::base::test::IsJson;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::Property;

TEST(SummaryOperatorTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<SummaryOperator, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          .desc = "missing",
          .json = R"json({})json",
          .matches = ValueIs(SummaryOperator::kCount),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"summary_operator": 1})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryOperatorValueInvalid),
      },
      {
          .desc = "invalid_value",
          .json = R"json({"summary_operator": "COUNT"})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryOperatorValueInvalid),
      },
      {
          .desc = "valid_count",
          .json = R"json({"summary_operator": "count"})json",
          .matches = ValueIs(SummaryOperator::kCount),
      },
      {
          .desc = "valid_value_sum",
          .json = R"json({"summary_operator": "value_sum"})json",
          .matches = ValueIs(SummaryOperator::kValueSum),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(ParseSummaryOperator(dict), test_case.matches);
  }
}

TEST(SummaryOperatorTest, Serialize) {
  {
    base::Value::Dict out;
    Serialize(SummaryOperator::kCount, out);
    EXPECT_THAT(out, IsJson(R"json({"summary_operator": "count"})json"));
  }

  {
    base::Value::Dict out;
    Serialize(SummaryOperator::kValueSum, out);
    EXPECT_THAT(out, IsJson(R"json({"summary_operator": "value_sum"})json"));
  }
}

TEST(SummaryBucketsTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    MaxEventLevelReports max_event_level_reports = MaxEventLevelReports::Max();
    ::testing::Matcher<base::expected<SummaryBuckets, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          .desc = "missing",
          .json = R"json({})json",
          .max_event_level_reports = MaxEventLevelReports(5),
          .matches = ValueIs(
              Property(&SummaryBuckets::starts, ElementsAre(1, 2, 3, 4, 5))),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"summary_buckets": 1})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsListInvalid),
      },
      {
          .desc = "empty",
          .json = R"json({"summary_buckets": []})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsListInvalid),
      },
      {
          .desc = "too_long",
          .json = R"json({"summary_buckets": [1, 2, 3, 4]})json",
          .max_event_level_reports = MaxEventLevelReports(3),
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsListInvalid),
      },
      {
          .desc = "value_wrong_type",
          .json = R"json({"summary_buckets": [0.1]})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsValueInvalid),
      },
      {
          .desc = "value_out_of_range",
          .json = R"json({"summary_buckets": [-1]})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsValueInvalid),
      },
      {
          .desc = "value_zero",
          .json = R"json({"summary_buckets": [0]})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsValueInvalid),
      },
      {
          .desc = "non_increasing",
          .json = R"json({"summary_buckets": [1, 3, 5, 2]})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsValueInvalid),
      },
      {
          .desc = "duplicate",
          .json = R"json({"summary_buckets": [1, 3, 3]})json",
          .matches =
              ErrorIs(SourceRegistrationError::kSummaryBucketsValueInvalid),
      },
      {
          .desc = "valid",
          .json = R"json({"summary_buckets": [1, 3, 5]})json",
          .max_event_level_reports = MaxEventLevelReports(3),
          .matches =
              ValueIs(Property(&SummaryBuckets::starts, ElementsAre(1, 3, 5))),
      },
      {
          .desc = "valid_max_uint32",
          .json = R"json({"summary_buckets": [4294967295]})json",
          .matches = ValueIs(
              Property(&SummaryBuckets::starts, ElementsAre(4294967295))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(SummaryBuckets::Parse(dict, test_case.max_event_level_reports),
                test_case.matches);
  }
}

TEST(SummaryBucketsTest, Serialize) {
  base::Value::Dict dict;
  SummaryBuckets({1, 3, 4294967295}).Serialize(dict);

  EXPECT_THAT(dict, IsJson(R"json({
    "summary_buckets": [1, 3, 4294967295]
  })json"));
}

}  // namespace
}  // namespace attribution_reporting
