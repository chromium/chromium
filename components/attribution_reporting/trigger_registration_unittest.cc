// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;

TriggerRegistration TriggerRegistrationWith(
    base::FunctionRef<void(TriggerRegistration&)> f) {
  TriggerRegistration r;
  f(r);
  return r;
}

TEST(TriggerRegistrationTest, Parse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<
        base::expected<TriggerRegistration, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          ErrorIs(TriggerRegistrationError::kInvalidJson),
      },
      {
          "root_wrong_type",
          "3",
          ErrorIs(TriggerRegistrationError::kRootWrongType),
      },
      {
          "empty",
          R"json({})json",
          ValueIs(AllOf(
              Field(&TriggerRegistration::filters, FilterPair()),
              Field(&TriggerRegistration::debug_key, std::nullopt),
              Field(&TriggerRegistration::aggregatable_dedup_keys, IsEmpty()),
              Field(&TriggerRegistration::event_triggers, IsEmpty()),
              Field(&TriggerRegistration::aggregatable_trigger_data, IsEmpty()),
              Field(&TriggerRegistration::aggregatable_values, IsEmpty()),
              Field(&TriggerRegistration::debug_reporting, false),
              Field(&TriggerRegistration::aggregation_coordinator_origin,
                    std::nullopt),
              Field(&TriggerRegistration::aggregatable_trigger_config,
                    AggregatableTriggerConfig()),
              Field(&TriggerRegistration::aggregatable_debug_reporting_config,
                    AggregatableDebugReportingConfig()))),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"], "_lookback_window": 2 }})json",
          ValueIs(Field(&TriggerRegistration::filters,
                        FilterPair(
                            /*positive=*/{*FilterConfig::Create(
                                {{{"a", {"b"}}}},
                                /*lookback_window=*/base::Seconds(2))},
                            /*negative=*/FiltersDisjunction()))),
      },
      {
          "filters_wrong_type",
          R"json({"filters": 5})json",
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_valid",
          R"json({"not_filters":{"a":["b"]}})json",
          ValueIs(Field(
              &TriggerRegistration::filters,
              FilterPair(
                  /*positive=*/FiltersDisjunction(),
                  /*negative=*/{*FilterConfig::Create({{{"a", {"b"}}}})}))),
      },
      {
          "not_filters_wrong_type",
          R"json({"not_filters": 5})json",
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "debug_key_valid",
          R"json({"debug_key":"5"})json",
          ValueIs(Field(&TriggerRegistration::debug_key, 5)),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5"})json",
          ValueIs(Field(&TriggerRegistration::debug_key, std::nullopt)),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5})json",
          ValueIs(Field(&TriggerRegistration::debug_key, std::nullopt)),
      },
      {
          "event_triggers_valid",
          R"json({"event_trigger_data":[{}, {"trigger_data":"5"}]})json",
          ValueIs(Field(&TriggerRegistration::event_triggers,
                        ElementsAre(EventTriggerData(),
                                    EventTriggerData(/*data=*/5, /*priority=*/0,
                                                     /*dedup_key=*/std::nullopt,
                                                     FilterPair())))),
      },
      {
          "event_triggers_wrong_type",
          R"json({"event_trigger_data":{}})json",
          ErrorIs(TriggerRegistrationError::kEventTriggerDataWrongType),
      },
      {
          "event_trigger_data_wrong_type",
          R"json({"event_trigger_data":["abc"]})json",
          ErrorIs(TriggerRegistrationError::kEventTriggerDataWrongType),
      },
      {
          "event_triggers_data_invalid",
          R"json({"event_trigger_data":[{"trigger_data":5}]})json",
          ErrorIs(TriggerRegistrationError::kEventTriggerDataValueInvalid),
      },
      {
          "event_triggers_priority_invalid",
          R"json({"event_trigger_data": [
                {
                  "priority":0
                }
              ]})json",
          ErrorIs(TriggerRegistrationError::kEventPriorityValueInvalid),
      },
      {
          "event_triggers_dedup_keys_invalid",
          R"json({"event_trigger_data": [
                {
                  "deduplication_key": 1
                }
              ]})json",
          ErrorIs(TriggerRegistrationError::kEventDedupKeyValueInvalid),
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
          ValueIs(Field(&TriggerRegistration::aggregatable_trigger_data,
                        ElementsAre(AggregatableTriggerData(
                                        /*key_piece=*/1,
                                        /*source_keys=*/{"a"}, FilterPair()),
                                    AggregatableTriggerData(
                                        /*key_piece=*/2,
                                        /*source_keys=*/{"b"}, FilterPair())))),
      },
      {
          "aggregatable_trigger_data_list_wrong_type",
          R"json({"aggregatable_trigger_data": {}})json",
          ErrorIs(TriggerRegistrationError::kAggregatableTriggerDataWrongType),
      },
      {
          "aggregatable_trigger_data_wrong_type",
          R"json({"aggregatable_trigger_data":["abc"]})json",
          ErrorIs(TriggerRegistrationError::kAggregatableTriggerDataWrongType),
      },
      {
          "aggregatable_values_valid",
          R"json({"aggregatable_values":{
           "a":1, "b": {"value": 2, "filtering_id": "3"}}})json",
          ValueIs(Field(&TriggerRegistration::aggregatable_values,
                        ElementsAre(*AggregatableValues::Create(
                            {{"a", *AggregatableValuesValue::Create(
                                       1, /*filtering_id=*/0u)},
                             {"b", *AggregatableValuesValue::Create(2, 3)}},
                            FilterPair())))),
      },
      {
          "aggregatable_values_invalid_filtering_id",
          R"json({"aggregatable_values":{
            "a":1,
            "b": {"value": 2, "filtering_id": "256"}
          }})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueInvalid),
      },
      {
          "aggregatable_values_list_invalid_filtering_id",
          R"json({"aggregatable_values":[{
            "values": {
              "a":1,
              "b": {"value": 2, "filtering_id": "256" }
            }
          }]})json",
          ErrorIs(
              TriggerRegistrationError::kAggregatableValuesListValueInvalid),
      },
      {
          "aggregatable_values_wrong_type",
          R"json({"aggregatable_values":123})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesWrongType),
      },
      {
          "debug_reporting_valid",
          R"json({"debug_reporting": true})json",
          ValueIs(Field(&TriggerRegistration::debug_reporting, true)),
      },
      {
          "debug_reporting_wrong_type",
          R"json({"debug_reporting":"true"})json",
          ValueIs(Field(&TriggerRegistration::debug_reporting, false)),
      },
      {
          "aggregatable_dedup_keys_valid",
          R"json({
            "aggregatable_deduplication_keys":[
              {},
              {"deduplication_key":"5"}
            ]
          })json",
          ValueIs(Field(&TriggerRegistration::aggregatable_dedup_keys,
                        ElementsAre(AggregatableDedupKey(),
                                    AggregatableDedupKey(/*dedup_key=*/5,
                                                         FilterPair())))),
      },
      {
          "aggregatable_dedup_keys_wrong_type",
          R"json({"aggregatable_deduplication_keys":{}})json",
          ErrorIs(TriggerRegistrationError::kAggregatableDedupKeyWrongType),
      },
      {
          "aggregatable_dedup_key_wrong_type",
          R"json({"aggregatable_deduplication_keys":["abc"]})json",
          ErrorIs(TriggerRegistrationError::kAggregatableDedupKeyWrongType),
      },
      {
          "aggregatable_dedup_key_invalid",
          R"json({"aggregatable_deduplication_keys":[
              {},
              {"deduplication_key":5}
            ]})json",
          ErrorIs(TriggerRegistrationError::kAggregatableDedupKeyValueInvalid),
      },
      {
          // Tested more thoroughly in
          // `aggregatable_trigger_config_unittest.cc`.
          "aggregatable_source_registration_time_valid",
          R"json({"aggregatable_source_registration_time":"include"})json",
          ValueIs(Field(
              &TriggerRegistration::aggregatable_trigger_config,
              Property(
                  &AggregatableTriggerConfig::source_registration_time_config,
                  SourceRegistrationTimeConfig::kInclude))),
      },
      {
          // Tested more thoroughly in
          // `aggregatable_trigger_config_unittest.cc`.
          "aggregatable_source_registration_time_invalid",
          R"json({"aggregatable_source_registration_time":123})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableSourceRegistrationTimeValueInvalid),
      },
      {
          // Tested more thoroughly in
          // `aggregatable_filtering_id_max_bytes_unittest.cc`
          "aggregatable_filtering_id_max_bytes_invalid",
          R"json({"aggregatable_filtering_id_max_bytes": null})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      }};

  static constexpr char kTriggerRegistrationErrorMetric[] =
      "Conversions.TriggerRegistrationError11";

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::HistogramTester histograms;

    auto trigger = TriggerRegistration::Parse(test_case.json);
    EXPECT_THAT(trigger, test_case.matches);

    if (trigger.has_value()) {
      histograms.ExpectTotalCount(kTriggerRegistrationErrorMetric, 0);
    } else {
      histograms.ExpectUniqueSample(kTriggerRegistrationErrorMetric,
                                    trigger.error(), 1);
    }
  }
}

TEST(TriggerRegistrationTest, ToJson) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
  const struct {
    TriggerRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          TriggerRegistration(),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "aggregatable_filtering_id_max_bytes": 1,
            "debug_reporting": false,
            "aggregatable_debug_reporting": {
              "key_piece": "0x0"
            }
          })json",
      },
      {
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregatable_dedup_keys = {
                AggregatableDedupKey(/*dedup_key=*/1, FilterPair())};
            r.aggregatable_trigger_data = {AggregatableTriggerData()};
            r.aggregatable_values = {
                *AggregatableValues::Create(
                    {{"a", *AggregatableValuesValue::Create(
                               2, /*filtering_id=*/0u)}},
                    FilterPair()),
                *AggregatableValues::Create(
                    {{"b", *AggregatableValuesValue::Create(
                               3, /*filtering_id=*/0u)}},
                    FilterPair())};
            r.debug_key = 3;
            r.debug_reporting = true;
            r.event_triggers = {EventTriggerData()};
            r.filters.positive = {*FilterConfig::Create({{{"b", {}}}})};
            r.filters.negative = {*FilterConfig::Create(
                {{{"c", {}}}}, /*lookback_window=*/base::Seconds(2))};
            r.aggregatable_trigger_config = *AggregatableTriggerConfig::Create(
                SourceRegistrationTimeConfig::kExclude,
                /*trigger_context_id=*/"123",
                *AggregatableFilteringIdsMaxBytes::Create(2));
            r.aggregatable_debug_reporting_config =
                AggregatableDebugReportingConfig(
                    /*key_piece=*/1,
                    /*debug_data=*/
                    {{mojom::DebugDataType::kTriggerNoMatchingSource,
                      *AggregatableDebugReportingContribution::Create(
                          /*key_piece=*/10, /*value=*/12)}},
                    /*aggregation_coordinator_origin=*/std::nullopt);
          }),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "aggregatable_deduplication_keys": [{"deduplication_key":"1"}],
            "aggregatable_filtering_id_max_bytes": 2,
            "aggregatable_trigger_data": [{"key_piece":"0x0"}],
            "aggregatable_values": [
              {"values":{"a": {"value": 2, "filtering_id": "0"}}},
              {"values":{"b": {"value": 3, "filtering_id": "0"}}}],
            "debug_key": "3",
            "debug_reporting": true,
            "event_trigger_data": [{"priority":"0","trigger_data":"0"}],
            "filters": [{"b": []}],
            "not_filters": [{"c": [], "_lookback_window": 2}],
            "trigger_context_id": "123",
            "aggregatable_debug_reporting": {
              "key_piece": "0x1",
              "debug_data": [
                {
                  "types": ["trigger-no-matching-source"],
                  "key_piece": "0xa",
                  "value": 12
                }
              ]
            }
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
    ::testing::Matcher<
        base::expected<TriggerRegistration, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "aggregation_coordinator_origin_valid",
          R"json({"aggregation_coordinator_origin":"https://a.test"})json",
          ValueIs(Field(&TriggerRegistration::aggregation_coordinator_origin,
                        *SuitableOrigin::Create(GURL("https://a.test")))),
      },
      {
          "aggregation_coordinator_origin_wrong_type",
          R"json({"aggregation_coordinator_origin":123})json",
          ErrorIs(
              TriggerRegistrationError::kAggregationCoordinatorValueInvalid),
      },
      {
          "aggregation_coordinator_origin_invalid_value",
          R"json({"aggregation_coordinator_origin":"https://b.test"})json",
          ErrorIs(
              TriggerRegistrationError::kAggregationCoordinatorValueInvalid),
      },
  };

  static constexpr char kTriggerRegistrationErrorMetric[] =
      "Conversions.TriggerRegistrationError11";

  aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::HistogramTester histograms;

    auto trigger = TriggerRegistration::Parse(test_case.json);
    EXPECT_THAT(trigger, test_case.matches);

    if (trigger.has_value()) {
      histograms.ExpectTotalCount(kTriggerRegistrationErrorMetric, 0);
    } else {
      histograms.ExpectUniqueSample(kTriggerRegistrationErrorMetric,
                                    trigger.error(), 1);
    }
  }
}

TEST(TriggerRegistrationTest, SerializeAggregationCoordinator) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
  const struct {
    TriggerRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          TriggerRegistration(),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "debug_reporting": false,
            "aggregatable_debug_reporting": {
              "key_piece": "0x0"
            }
          })json",
      },
      {
          TriggerRegistrationWith([](TriggerRegistration& r) {
            r.aggregation_coordinator_origin =
                SuitableOrigin::Create(GURL("https://a.test"));
          }),
          R"json({
            "aggregatable_source_registration_time": "exclude",
            "aggregation_coordinator_origin": "https://a.test",
            "debug_reporting": false,
            "aggregatable_debug_reporting": {
              "key_piece": "0x0"
            }
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(TriggerRegistrationTest, ParseAggregatableDebugReportingConfig) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<TriggerRegistration, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "valid",
          R"json({
            "aggregatable_debug_reporting": {
              "key_piece": "0x2"
            }
          })json",
          ValueIs(
              Field(&TriggerRegistration::aggregatable_debug_reporting_config,
                    Field(&AggregatableDebugReportingConfig::key_piece, 2))),
      },
      {
          "invalid",
          R"json({
            "aggregatable_debug_reporting": ""
          })json",
          ValueIs(
              Field(&TriggerRegistration::aggregatable_debug_reporting_config,
                    AggregatableDebugReportingConfig())),
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableDebugReporting);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    EXPECT_THAT(TriggerRegistration::Parse(test_case.json), test_case.matches);
  }
}

TEST(TriggerRegistrationTest, ParseAttributionScopesConfig) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<TriggerRegistration, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "attribution_scopes_set_valid",
          R"json({
            "attribution_scopes": ["123"]
          })json",
          ValueIs(Field(&TriggerRegistration::attribution_scopes,
                        AttributionScopesSet({"123"}))),
      },
      {
          "empty",
          R"json({})json",
          ValueIs(Field(&TriggerRegistration::attribution_scopes,
                        AttributionScopesSet())),
      },
      {
          "attribution_scopes_set_invalid",
          R"json({
            "attribution_scopes": [123]
          })json",
          ErrorIs(TriggerRegistrationError::kAttributionScopesValueInvalid),
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionScopes);

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    SCOPED_TRACE(test_case.desc);

    auto trigger = TriggerRegistration::Parse(test_case.json);
    EXPECT_THAT(trigger, test_case.matches);

    if (trigger.has_value()) {
      histograms.ExpectUniqueSample("Conversions.ScopesPerTriggerRegistration",
                                    trigger->attribution_scopes.scopes().size(),
                                    1);
    }
  }
}

}  // namespace
}  // namespace attribution_reporting
