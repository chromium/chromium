// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/aggregation_service/features.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

TriggerRegistration TriggerRegistrationWith(
    base::FunctionRef<void(TriggerRegistration&)> f) {
  TriggerRegistration r;
  f(r);
  return r;
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
          R"json({"filters":{"a":["b"], "_lookback_window": 2 }})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.filters.positive = {*FilterConfig::Create(
                {{{"a", {"b"}}}}, /*lookback_window=*/base::Seconds(2))};
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
            r.filters.negative = {*FilterConfig::Create({{{"a", {"b"}}}})};
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
            r.event_triggers = {
                EventTriggerData(),
                EventTriggerData(/*data=*/5, /*priority=*/0,
                                 /*dedup_key=*/absl::nullopt, FilterPair())};
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
          "event_triggers_data_invalid",
          R"json({"event_trigger_data":[{"trigger_data":5}]})json",
          base::unexpected(
              TriggerRegistrationError::kEventTriggerDataValueInvalid),
      },
      {
          "event_triggers_priority_invalid",
          R"json({"event_trigger_data": [
                {
                  "priority":0
                }
              ]})json",
          base::unexpected(
              TriggerRegistrationError::kEventPriorityValueInvalid),
      },
      {
          "event_triggers_dedup_keys_invalid",
          R"json({"event_trigger_data": [
                {
                  "deduplication_key": 1
                }
              ]})json",
          base::unexpected(
              TriggerRegistrationError::kEventDedupKeyValueInvalid),
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
            r.aggregatable_trigger_data = {
                *AggregatableTriggerData::Create(
                    /*key_piece=*/1,
                    /*source_keys=*/{"a"}, FilterPair()),
                *AggregatableTriggerData::Create(
                    /*key_piece=*/2,
                    /*source_keys=*/{"b"}, FilterPair())};
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
          "aggregatable_dedup_keys_valid",
          R"json({
            "aggregatable_deduplication_keys":[
              {},
              {"deduplication_key":"5"}
            ]
          })json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_dedup_keys = {
                AggregatableDedupKey(),
                AggregatableDedupKey(/*dedup_key=*/5, FilterPair())};
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
      {
          "aggregatable_dedup_key_invalid",
          R"json({"aggregatable_deduplication_keys":[
              {},
              {"deduplication_key":5}
            ]})json",
          base::unexpected(
              TriggerRegistrationError::kAggregatableDedupKeyValueInvalid),
      },
      {
          "aggregatable_source_registration_time_include",
          R"json({"aggregatable_source_registration_time":"include"})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.source_registration_time_config =
                mojom::SourceRegistrationTimeConfig::kInclude;
          }),
      },
      {
          "aggregatable_source_registration_time_exclude",
          R"json({"aggregatable_source_registration_time":"exclude"})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.source_registration_time_config =
                mojom::SourceRegistrationTimeConfig::kExclude;
          }),
      },
      {
          "aggregatable_source_registration_time_wrong_type",
          R"json({"aggregatable_source_registration_time":123})json",
          base::unexpected(TriggerRegistrationError::
                               kAggregatableSourceRegistrationTimeWrongType),
      },
      {
          "aggregatable_source_registration_time_invalid_value",
          R"json({"aggregatable_source_registration_time":"unknown"})json",
          base::unexpected(TriggerRegistrationError::
                               kAggregatableSourceRegistrationTimeUnknownValue),
      },
  };

  static constexpr char kTriggerRegistrationErrorMetric[] =
      "Conversions.TriggerRegistrationError6";

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

TEST(TriggerRegistrationTest, ToJson) {
  const struct {
    TriggerRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          TriggerRegistration(),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "debug_reporting": false
          })json",
      },
      {
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_dedup_keys = {
                AggregatableDedupKey(/*dedup_key=*/1, FilterPair())};
            r.aggregatable_trigger_data = {AggregatableTriggerData()};
            r.aggregatable_values = *AggregatableValues::Create({{"a", 2}});
            r.debug_key = 3;
            r.debug_reporting = true;
            r.event_triggers = {EventTriggerData()};
            r.filters.positive = {*FilterConfig::Create({{{"b", {}}}})};
            r.filters.negative = {*FilterConfig::Create(
                {{{"c", {}}}}, /*lookback_window=*/base::Seconds(2))};
            r.source_registration_time_config =
                mojom::SourceRegistrationTimeConfig::kInclude;
          }),
          R"json({
            "aggregatable_source_registration_time": "include",
            "aggregatable_deduplication_keys": [{"deduplication_key":"1"}],
            "aggregatable_trigger_data": [{"key_piece":"0x0"}],
            "aggregatable_values": {"a": 2},
            "debug_key": "3",
            "debug_reporting": true,
            "event_trigger_data": [{"priority":"0","trigger_data":"0"}],
            "filters": [{"b": []}],
            "not_filters": [{"c": [], "_lookback_window": 2}]
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(TriggerRegistrationTest, ParseAggregationCoordinator) {
  const struct {
    const char* description;
    const char* json;
    base::expected<TriggerRegistration, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "aggregation_coordinator_origin_valid",
          R"json({"aggregation_coordinator_origin":"https://aws.example.test"})json",
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregation_coordinator_origin =
                SuitableOrigin::Create(GURL("https://aws.example.test"));
          }),
      },
      {
          "aggregation_coordinator_origin_wrong_type",
          R"json({"aggregation_coordinator_origin":123})json",
          base::unexpected(
              TriggerRegistrationError::kAggregationCoordinatorWrongType),
      },
      {
          "aggregation_coordinator_origin_invalid_value",
          R"json({"aggregation_coordinator_origin":"https://unknown.example.test"})json",
          base::unexpected(
              TriggerRegistrationError::kAggregationCoordinatorUnknownValue),
      },
  };

  static constexpr char kTriggerRegistrationErrorMetric[] =
      "Conversions.TriggerRegistrationError6";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      aggregation_service::kAggregationServiceMultipleCloudProviders,
      {{"aws_cloud", "https://aws.example.test"}});

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

TEST(TriggerRegistrationTest, SerializeAggregationCoordinator) {
  const struct {
    TriggerRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          TriggerRegistration(),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "debug_reporting": false
          })json",
      },
      {
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregation_coordinator_origin =
                SuitableOrigin::Create(GURL("https://aws.example.test"));
          }),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "aggregation_coordinator_origin": "https://aws.example.test",
            "debug_reporting": false
          })json",
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      aggregation_service::kAggregationServiceMultipleCloudProviders);

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
