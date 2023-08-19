// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_dedup_key.h"

#include "base/functional/function_ref.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

AggregatableDedupKey AggregatableDedupKeyWith(
    base::FunctionRef<void(AggregatableDedupKey&)> f) {
  AggregatableDedupKey key;
  f(key);
  return key;
}

TEST(AggregatableDedupKeyTest, FromJSON) {
  const struct {
    const char* description;
    const char* json;
    base::expected<AggregatableDedupKey, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          AggregatableDedupKey(),
      },
      {
          "dedup_key_valid",
          R"json({"deduplication_key":"3"})json",
          AggregatableDedupKeyWith(
              [](AggregatableDedupKey& key) { key.dedup_key = 3; }),
      },
      {
          "dedup_key_wrong_type",
          R"json({"deduplication_key":123})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableDedupKeyValueInvalid),
      },
      {
          "dedup_key_invalid",
          R"json({"deduplication_key":"abc"})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableDedupKeyValueInvalid),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"], "_lookback_window": 1}})json",
          AggregatableDedupKeyWith([](AggregatableDedupKey& key) {
            key.filters.positive = {*FilterConfig::Create(
                {{{"a", {"b"}}}}, /*lookback_window=*/base::Seconds(1))};
          }),
      },
      {
          "filters_wrong_type",
          R"json({"filters":123})json",
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_valid",
          R"json({"not_filters":{"a":["b"], "_lookback_window": 1}})json",
          AggregatableDedupKeyWith([](AggregatableDedupKey& key) {
            key.filters.negative = {*FilterConfig::Create(
                {{{"a", {"b"}}}}, /*lookback_window=*/base::Seconds(1))};
          }),
      },
      {
          "not_filters_wrong_type",
          R"json({"not_filters":123})json",
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_EQ(AggregatableDedupKey::FromJSON(value), test_case.expected)
        << test_case.description;
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
