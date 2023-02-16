// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

TriggerRegistration TriggerRegistrationWith(
    base::FunctionRef<void(TriggerRegistration&)> f) {
  TriggerRegistration r;
  f(r);
  return r;
}

base::expected<TriggerRegistration, TriggerRegistrationError>
ParseWithAggregatableTriggerData(size_t n) {
  base::Value::List list;
  for (size_t i = 0; i < n; ++i) {
    base::Value::Dict data;
    data.Set("key_piece", "0x1");

    base::Value::List source_keys;
    source_keys.Append("abc");
    data.Set("source_keys", std::move(source_keys));

    list.Append(std::move(data));
  }

  base::Value::Dict dict;
  dict.Set("aggregatable_trigger_data", std::move(list));
  return TriggerRegistration::Parse(std::move(dict));
}

TEST(TriggerRegistrationTest, Parse) {
  const struct {
    const char* description;
    const char* json;
    base::expected<TriggerRegistration, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          base::unexpected(TriggerRegistrationError::kInvalidJson),
      },
      {
          "root_wrong_type",
          "3",
          base::unexpected(TriggerRegistrationError::kRootWrongType),
      },
      {
          "empty",
          R"json({})json",
          TriggerRegistration(),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"]}})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.filters.positive = *Filters::Create({{"a", {"b"}}});
          }),
      },
      {
          "filters_wrong_type",
          R"json({"filters": 5})json",
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_valid",
          R"json({"not_filters":{"a":["b"]}})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.filters.negative = *Filters::Create({{"a", {"b"}}});
          }),
      },
      {
          "not_filters_wrong_type",
          R"json({"not_filters": 5})json",
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "debug_key_valid",
          R"json({"debug_key":"5"})json",
          TriggerRegistrationWith(
              [](TriggerRegistration& r) { r.debug_key = 5; }),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5"})json",
          TriggerRegistration(),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5})json",
          TriggerRegistration(),
      },
      {
          "event_triggers_valid",
          R"json({"event_trigger_data":[{}, {"trigger_data":"5"}]})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.event_triggers = *EventTriggerDataList::Create(
                {EventTriggerData(),
                 EventTriggerData(/*data=*/5, /*priority=*/0,
                                  /*dedup_key=*/absl::nullopt, FilterPair())});
          }),
      },
      {
          "event_triggers_wrong_type",
          R"json({"event_trigger_data":{}})json",
          base::unexpected(
              TriggerRegistrationError::kEventTriggerDataListWrongType),
      },
      {
          "event_trigger_data_wrong_type",
          R"json({"event_trigger_data":["abc"]})json",
          base::unexpected(
              TriggerRegistrationError::kEventTriggerDataWrongType),
      },
      {
          "aggregatable_trigger_data_valid",
          R"json({
          "aggregatable_trigger_data":[
            {
              "key_piece": "0x1",
              "source_keys": ["a"]
            },
            {
              "key_piece": "0x2",
              "source_keys": ["b"]
            }
          ]
        })json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_trigger_data = *AggregatableTriggerDataList::Create(
                {*AggregatableTriggerData::Create(
                     /*key_piece=*/1,
                     /*source_keys=*/{"a"}, FilterPair()),
                 *AggregatableTriggerData::Create(
                     /*key_piece=*/2,
                     /*source_keys=*/{"b"}, FilterPair())});
          }),
      },
      {
          "aggregatable_trigger_data_list_wrong_type",
          R"json({"aggregatable_trigger_data": {}})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableTriggerDataListWrongType),
      },
      {
          "aggregatable_trigger_data_wrong_type",
          R"json({"aggregatable_trigger_data":["abc"]})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableTriggerDataWrongType),
      },
      {
          "aggregatable_values_valid",
          R"json({"aggregatable_values":{"a":1}})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_values = *AggregatableValues::Create({{"a", 1}});
          }),
      },
      {
          "aggregatable_values_wrong_type",
          R"json({"aggregatable_values":123})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableValuesWrongType),
      },
      {
          "debug_reporting_valid",
          R"json({"debug_reporting": true})json",
          TriggerRegistrationWith(
              [](TriggerRegistration& r) { r.debug_reporting = true; }),
      },
      {
          "debug_reporting_wrong_type",
          R"json({"debug_reporting":"true"})json",
          TriggerRegistration(),
      },
      {
          "aggregation_coordinator_identifier_valid",
          R"json({"aggregation_coordinator_identifier":"aws-cloud"})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregation_coordinator =
                aggregation_service::mojom::AggregationCoordinator::kAwsCloud;
          }),
      },
      {
          "aggregation_coordinator_identifier_wrong_type",
          R"json({"aggregation_coordinator_identifier":123})json",
          base::unexpected(
              TriggerRegistrationError::kAggregationCoordinatorWrongType),
      },
      {
          "aggregation_coordinator_identifier_invalid_value",
          R"json({"aggregation_coordinator_identifier":"unknown"})json",
          base::unexpected(
              TriggerRegistrationError::kAggregationCoordinatorUnknownValue),
      },
      {
          "aggregatable_dedup_keys_valid",
          R"json({
            "aggregatable_deduplication_keys":[
              {},
              {"deduplication_key":"5"}
            ]
          })json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_dedup_keys = *AggregatableDedupKeyList::Create(
                {AggregatableDedupKey(),
                 AggregatableDedupKey(/*dedup_key=*/5, FilterPair())});
          }),
      },
      {
          "aggregatable_dedup_keys_wrong_type",
          R"json({"aggregatable_deduplication_keys":{}})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableDedupKeyListWrongType),
      },
      {
          "aggregatable_dedup_key_wrong_type",
          R"json({"aggregatable_deduplication_keys":["abc"]})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableDedupKeyWrongType),
      },
  };

  static constexpr char kTriggerRegistrationErrorMetric[] =
      "Conversions.TriggerRegistrationError2";

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    auto trigger = TriggerRegistration::Parse(test_case.json);
    EXPECT_EQ(trigger, test_case.expected) << test_case.description;

    if (trigger.has_value()) {
      histograms.ExpectTotalCount(kTriggerRegistrationErrorMetric, 0);
    } else {
      histograms.ExpectUniqueSample(kTriggerRegistrationErrorMetric,
                                    trigger.error(), 1);
    }
  }
}

TEST(TriggerRegistrationTest, Parse_EventTriggerDataCount) {
  const auto parse_with_event_triggers = [&](size_t n) {
    base::Value::List list;
    for (size_t i = 0; i < n; ++i) {
      list.Append(base::Value::Dict());
    }

    base::Value::Dict dict;
    dict.Set("event_trigger_data", std::move(list));
    return TriggerRegistration::Parse(std::move(dict));
  };

  for (size_t count = 0; count <= kMaxEventTriggerData; ++count) {
    EXPECT_TRUE(parse_with_event_triggers(count).has_value());
  }

  EXPECT_EQ(
      parse_with_event_triggers(kMaxEventTriggerData + 1),
      base::unexpected(TriggerRegistrationError::kEventTriggerDataListTooLong));
}

TEST(TriggerRegistrationTest, Parse_AggregatableTriggerDataCount) {
  for (size_t count = 0; count <= kMaxAggregatableTriggerDataPerTrigger;
       ++count) {
    EXPECT_TRUE(ParseWithAggregatableTriggerData(count).has_value());
  }

  EXPECT_EQ(ParseWithAggregatableTriggerData(
                kMaxAggregatableTriggerDataPerTrigger + 1),
            base::unexpected(
                TriggerRegistrationError::kAggregatableTriggerDataListTooLong));
}

TEST(TriggerRegistrationTest, Parse_AggregatableDedupKeyCount) {
  const auto parse_with_aggregatable_dedup_key = [&](size_t n) {
    base::Value::List list;
    for (size_t i = 0; i < n; ++i) {
      list.Append(base::Value::Dict());
    }

    base::Value::Dict dict;
    dict.Set("aggregatable_deduplication_keys", std::move(list));
    return TriggerRegistration::Parse(std::move(dict));
  };

  for (size_t count = 0; count <= kMaxAggregatableDedupKeys; ++count) {
    EXPECT_TRUE(parse_with_aggregatable_dedup_key(count).has_value());
  }

  EXPECT_EQ(parse_with_aggregatable_dedup_key(kMaxAggregatableDedupKeys + 1),
            base::unexpected(
                TriggerRegistrationError::kAggregatableDedupKeyListTooLong));
}

TEST(TriggerRegistrationTest, Parse_RecordsMetrics) {
  using ::base::Bucket;
  using ::testing::ElementsAre;

  base::HistogramTester histograms;

  for (size_t count : {
           0,
           1,
           1,
           3,
       }) {
    ASSERT_TRUE(ParseWithAggregatableTriggerData(count).has_value());
  }

  ASSERT_FALSE(ParseWithAggregatableTriggerData(
                   kMaxAggregatableTriggerDataPerTrigger + 1)
                   .has_value());

  EXPECT_THAT(
      histograms.GetAllSamples("Conversions.AggregatableTriggerDataLength"),
      ElementsAre(Bucket(0, 1), Bucket(1, 2), Bucket(3, 1)));
}

TEST(TriggerRegistrationTest, ToJson) {
  const struct {
    TriggerRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          TriggerRegistration(),
          R"json({
            "aggregation_coordinator_identifier": "aws-cloud",
            "debug_reporting": false
          })json",
      },
      {
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_dedup_keys = *AggregatableDedupKeyList::Create(
                {AggregatableDedupKey(/*dedup_key=*/1, FilterPair())});
            r.aggregatable_trigger_data = *AggregatableTriggerDataList::Create(
                {AggregatableTriggerData()});
            r.aggregatable_values = *AggregatableValues::Create({{"a", 2}});
            r.debug_key = 3;
            r.debug_reporting = true;
            r.event_triggers =
                *EventTriggerDataList::Create({EventTriggerData()});
            r.filters.positive = *Filters::Create({{"b", {}}});
            r.filters.negative = *Filters::Create({{"c", {}}});
          }),
          R"json({
            "aggregation_coordinator_identifier": "aws-cloud",
            "aggregatable_deduplication_keys": [{"deduplication_key":"1"}],
            "aggregatable_trigger_data": [{"key_piece":"0x0"}],
            "aggregatable_values": {"a": 2},
            "debug_key": "3",
            "debug_reporting": true,
            "event_trigger_data": [{"priority":"0","trigger_data":"0"}],
            "filters": {"b": []},
            "not_filters": {"c": []}
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
