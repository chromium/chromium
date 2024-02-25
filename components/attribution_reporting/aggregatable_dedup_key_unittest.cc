// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_dedup_key.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::Field;

TEST(AggregatableDedupKeyTest, FromJSON) {
  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<
        base::expected<AggregatableDedupKey, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(AllOf(Field(&AggregatableDedupKey::dedup_key, std::nullopt),
                        Field(&AggregatableDedupKey::filters, FilterPair()))),
      },
      {
          "dedup_key_valid",
          R"json({"deduplication_key":"3"})json",
          ValueIs(Field(&AggregatableDedupKey::dedup_key, 3)),
      },
      {
          "dedup_key_wrong_type",
          R"json({"deduplication_key":123})json",
          ErrorIs(TriggerRegistrationError::kAggregatableDedupKeyValueInvalid),
      },
      {
          "dedup_key_invalid",
          R"json({"deduplication_key":"abc"})json",
          ErrorIs(TriggerRegistrationError::kAggregatableDedupKeyValueInvalid),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"], "_lookback_window": 1}})json",
          ValueIs(Field(&AggregatableDedupKey::filters,
                        FilterPair(/*positive=*/{*FilterConfig::Create(
                                       {{{"a", {"b"}}}},
                                       /*lookback_window=*/base::Seconds(1))},
                                   /*negative=*/FiltersDisjunction()))),
      },
      {
          "filters_wrong_type",
          R"json({"filters":123})json",
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_valid",
          R"json({"not_filters":{"a":["b"], "_lookback_window": 1}})json",
          ValueIs(Field(&AggregatableDedupKey::filters,
                        FilterPair(
                            /*positive=*/FiltersDisjunction(),
                            /*negative=*/{*FilterConfig::Create(
                                {{{"a", {"b"}}}},
                                /*lookback_window=*/base::Seconds(1))}))),
      },
      {
          "not_filters_wrong_type",
          R"json({"not_filters":123})json",
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AggregatableDedupKey::FromJSON(value), test_case.matches);
  }
}

TEST(AggregatableDedupKeyTest, ToJson) {
  const struct {
    AggregatableDedupKey input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableDedupKey(),
          R"json({})json",
      },
      {
          AggregatableDedupKey(
              /*dedup_key=*/3,
              FilterPair(/*positive=*/{*FilterConfig::Create({{"a", {}}})},
                         /*negative=*/{*FilterConfig::Create({{"b", {}}})})),
          R"json({
            "deduplication_key": "3",
            "filters": [{"a": []}],
            "not_filters": [{"b": []}]
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
