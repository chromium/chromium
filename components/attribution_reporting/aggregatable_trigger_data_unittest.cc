// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_data.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

TEST(AggregatableTriggerDataTest, FromJSON) {
  const auto make_aggregatable_trigger_data_with_keys = [](size_t n) {
    base::Value::Dict dict;
    dict.Set("key_piece", "0x1");

    base::Value::List list;
    for (size_t i = 0; i < n; ++i) {
      list.Append(base::NumberToString(i));
    }
    dict.Set("source_keys", std::move(list));
    return base::Value(std::move(dict));
  };

  const auto make_aggregatable_trigger_data_with_key_length = [](size_t n) {
    base::Value::Dict dict;
    dict.Set("key_piece", "0x1");

    base::Value::List list;
    list.Append(std::string(n, 'a'));
    dict.Set("source_keys", std::move(list));
    return base::Value(std::move(dict));
  };

  struct {
    const char* description;
    base::Value json;
    base::expected<AggregatableTriggerData, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "required_fields_only",
          base::test::ParseJson(R"json({
            "key_piece": "0x1234"
          })json"),
          *AggregatableTriggerData::Create(
              /*key_piece=*/4660, /*source_keys=*/{}, FilterPair()),
      },
      {
          "empty_source_keys",
          base::test::ParseJson(R"json({
            "key_piece": "0x1234",
            "source_keys": []
          })json"),
          *AggregatableTriggerData::Create(
              /*key_piece=*/4660, /*source_keys=*/{}, FilterPair()),
      },
      {
          "non_empty_source_keys",
          base::test::ParseJson(R"json({
            "key_piece": "0x1234",
            "source_keys": ["a", "b"]
          })json"),
          *AggregatableTriggerData::Create(
              /*key_piece=*/4660, /*source_keys=*/{"a", "b"}, FilterPair()),
      },
      {
          "filters",
          base::test::ParseJson(R"json({
            "key_piece": "0x1",
            "filters": {"a": ["b", "c"]}
         })json"),
          *AggregatableTriggerData::Create(
              /*key_piece=*/1, /*source_keys=*/{},
              FilterPair{.positive = *Filters::Create({{"a", {"b", "c"}}})}),
      },
      {
          "not_filters",
          base::test::ParseJson(R"json({
            "key_piece": "0x2",
            "not_filters": {"a": ["b", "c"]}
          })json"),
          *AggregatableTriggerData::Create(
              /*key_piece=*/2, /*source_keys=*/{},
              FilterPair{.negative = *Filters::Create({{"a", {"b", "c"}}})}),
      },
      {
          "not_dictionary",
          base::Value(base::Value::List()),
          base::unexpected(
              TriggerRegistrationError::kAggregatableTriggerDataWrongType),
      },
      {
          "key_piece_missing",
          base::Value(base::Value::Dict()),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataKeyPieceMissing),
      },
      {
          "key_piece_wrong_type",
          base::test::ParseJson(R"json({"key_piece":123})json"),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataKeyPieceWrongType),
      },
      {
          "key_piece_wrong_format",
          base::test::ParseJson(R"json({"key_piece":"1234"})json"),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataKeyPieceWrongFormat),
      },
      {
          "source_keys_wrong_type",
          base::test::ParseJson(
              R"json({"key_piece":"0x1234", "source_keys":{}})json"),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataSourceKeysWrongType),
      },
      {
          "source_keys_key_wrong_type",
          base::test::ParseJson(
              R"json({"key_piece":"0x1234", "source_keys":[123]})json"),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataSourceKeysKeyWrongType),
      },
      {
          "source_keys_too_many_keys",
          make_aggregatable_trigger_data_with_keys(
              kMaxAggregationKeysPerSourceOrTrigger + 1),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataSourceKeysTooManyKeys),
      },
      {
          "source_keys_key_too_long",
          make_aggregatable_trigger_data_with_key_length(
              kMaxBytesPerAggregationKeyId + 1),
          base::unexpected(TriggerRegistrationError::
                               kAggregatableTriggerDataSourceKeysKeyTooLong),
      },
      {
          "filters_wrong_type",
          base::test::ParseJson(R"json({
            "key_piece": "0x1",
            "source_keys": ["abc"],
            "filters": 123
          })json"),
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_wrong_type",
          base::test::ParseJson(R"json({
            "key_piece": "0x1",
            "source_keys": ["abc"],
            "not_filters": 123
          })json"),
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
  };

  for (auto& test_case : kTestCases) {
    EXPECT_EQ(AggregatableTriggerData::FromJSON(test_case.json),
              test_case.expected)
        << test_case.description;
  }

  {
    base::Value json = make_aggregatable_trigger_data_with_keys(
        kMaxAggregationKeysPerSourceOrTrigger);
    EXPECT_TRUE(AggregatableTriggerData::FromJSON(json).has_value());
  }

  {
    base::Value json = make_aggregatable_trigger_data_with_key_length(
        kMaxBytesPerAggregationKeyId);
    EXPECT_TRUE(AggregatableTriggerData::FromJSON(json).has_value());
  }
}

TEST(AggregatableTriggerDataTest, ToJson) {
  const struct {
    AggregatableTriggerData input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableTriggerData(),
          R"json({"key_piece":"0x0"})json",
      },
      {
          *AggregatableTriggerData::Create(
              /*key_piece=*/1,
              /*source_keys=*/{"a", "b"},
              FilterPair{.positive = *Filters::Create({{"c", {}}}),
                         .negative = *Filters::Create({{"d", {}}})}),
          R"json({
            "key_piece":"0x1",
            "source_keys": ["a", "b"],
            "filters": {"c": []},
            "not_filters": {"d": []}
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
