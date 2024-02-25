// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_data.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;

TEST(AggregatableTriggerDataTest, FromJSON) {
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
    ::testing::Matcher<
        base::expected<AggregatableTriggerData, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "required_fields_only",
          base::test::ParseJson(R"json({
            "key_piece": "0x1234"
          })json"),
          ValueIs(
              AllOf(Property(&AggregatableTriggerData::key_piece, 4660),
                    Property(&AggregatableTriggerData::source_keys, IsEmpty()),
                    Property(&AggregatableTriggerData::filters, FilterPair()))),
      },
      {
          "empty_source_keys",
          base::test::ParseJson(R"json({
            "key_piece": "0x1234",
            "source_keys": []
          })json"),
          ValueIs(Property(&AggregatableTriggerData::source_keys, IsEmpty())),
      },
      {
          "non_empty_source_keys",
          base::test::ParseJson(R"json({
            "key_piece": "0x1234",
            "source_keys": ["a", "b"]
          })json"),
          ValueIs(Property(&AggregatableTriggerData::source_keys,
                           ElementsAre("a", "b"))),
      },
      {
          "filters",
          base::test::ParseJson(R"json({
            "key_piece": "0x1",
            "filters": {"a": ["b", "c"], "_lookback_window": 1}
         })json"),
          ValueIs(Property(&AggregatableTriggerData::filters,
                           FilterPair(
                               /*positive=*/{*FilterConfig::Create(
                                   {{"a", {"b", "c"}}},
                                   /*lookback_window=*/base::Seconds(1))},
                               /*negative=*/FiltersDisjunction()))),
      },
      {
          "not_filters",
          base::test::ParseJson(R"json({
            "key_piece": "0x2",
            "not_filters": {"a": ["b", "c"], "_lookback_window": 1 }
          })json"),
          ValueIs(
              Property(&AggregatableTriggerData::filters,
                       FilterPair(/*positive=*/FiltersDisjunction(),
                                  /*negative=*/{*FilterConfig::Create(
                                      {{"a", {"b", "c"}}},
                                      /*lookback_window=*/base::Seconds(1))}))),
      },
      {
          "not_dictionary",
          base::Value(base::Value::List()),
          ErrorIs(TriggerRegistrationError::kAggregatableTriggerDataWrongType),
      },
      {
          "key_piece_missing",
          base::Value(base::Value::Dict()),
          ErrorIs(TriggerRegistrationError::
                      kAggregatableTriggerDataKeyPieceMissing),
      },
      {
          "key_piece_wrong_type",
          base::test::ParseJson(R"json({"key_piece":123})json"),
          ErrorIs(TriggerRegistrationError::
                      kAggregatableTriggerDataKeyPieceWrongType),
      },
      {
          "key_piece_wrong_format",
          base::test::ParseJson(R"json({"key_piece":"1234"})json"),
          ErrorIs(TriggerRegistrationError::
                      kAggregatableTriggerDataKeyPieceWrongFormat),
      },
      {
          "source_keys_wrong_type",
          base::test::ParseJson(
              R"json({"key_piece":"0x1234", "source_keys":{}})json"),
          ErrorIs(TriggerRegistrationError::
                      kAggregatableTriggerDataSourceKeysWrongType),
      },
      {
          "source_keys_key_wrong_type",
          base::test::ParseJson(
              R"json({"key_piece":"0x1234", "source_keys":[123]})json"),
          ErrorIs(TriggerRegistrationError::
                      kAggregatableTriggerDataSourceKeysKeyWrongType),
      },
      {
          "source_keys_key_too_long",
          make_aggregatable_trigger_data_with_key_length(
              kMaxBytesPerAggregationKeyId + 1),
          ErrorIs(TriggerRegistrationError::
                      kAggregatableTriggerDataSourceKeysKeyTooLong),
      },
      {
          "filters_wrong_type",
          base::test::ParseJson(R"json({
            "key_piece": "0x1",
            "source_keys": ["abc"],
            "filters": 123
          })json"),
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_wrong_type",
          base::test::ParseJson(R"json({
            "key_piece": "0x1",
            "source_keys": ["abc"],
            "not_filters": 123
          })json"),
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
  };

  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_THAT(AggregatableTriggerData::FromJSON(test_case.json),
                test_case.matches);
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
              FilterPair(
                  /*positive=*/{*FilterConfig::Create(
                      {{"c", {}}}, /*lookback_window=*/base::Seconds(2))},
                  /*negative=*/{*FilterConfig::Create({{"d", {}}})})),
          R"json({
            "key_piece":"0x1",
            "source_keys": ["a", "b"],
            "filters": [{"c": [], "_lookback_window": 2}],
            "not_filters": [{"d": []}]
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
