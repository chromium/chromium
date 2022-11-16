// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/invoke.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

template <typename F>
TriggerRegistration TriggerRegistrationWith(SuitableOrigin reporting_origin,
                                            F&& f) {
  TriggerRegistration r(std::move(reporting_origin));
  base::invoke<F, TriggerRegistration&>(std::move(f), r);
  return r;
}

TEST(TriggerRegistrationTest, Parse) {
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://r.example");

  const struct {
    const char* description;
    const char* json;
    base::expected<TriggerRegistration, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          TriggerRegistration(reporting_origin),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"]}})json",
          TriggerRegistrationWith(
              reporting_origin,
              [](auto& r) {
                r.filters = *Filters::Create({{"a", {"b"}}});
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
          TriggerRegistrationWith(
              reporting_origin,
              [](auto& r) {
                r.not_filters = *Filters::Create({{"a", {"b"}}});
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
          TriggerRegistrationWith(reporting_origin,
                                  [](auto& r) { r.debug_key = 5; }),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5"})json",
          TriggerRegistration(reporting_origin),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5})json",
          TriggerRegistration(reporting_origin),
      },
      {
          "aggregatable_dedup_key_valid",
          R"json({"aggregatable_deduplication_key":"10"})json",
          TriggerRegistrationWith(
              reporting_origin, [](auto& r) { r.aggregatable_dedup_key = 10; }),
      },
      {
          "aggregatable_dedup_key_invalid",
          R"json({"aggregatable_deduplication_key":"-10"})json",
          TriggerRegistration(reporting_origin),
      },
      {
          "aggregatable_dedup_key_wrong_type",
          R"json({"aggregatable_deduplication_key": 10})json",
          TriggerRegistration(reporting_origin),
      },
      {
          "event_triggers_valid",
          R"json({"event_trigger_data":[{}, {"trigger_data":"5"}]})json",
          TriggerRegistrationWith(
              reporting_origin,
              [](auto& r) {
                r.event_triggers = *EventTriggerDataList::Create(
                    {EventTriggerData(),
                     EventTriggerData(/*data=*/5, /*priority=*/0,
                                      /*dedup_key=*/absl::nullopt,
                                      /*filters=*/Filters(),
                                      /*not_filters=*/Filters())});
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
          TriggerRegistrationWith(reporting_origin,
                                  [](auto& r) {
                                    r.aggregatable_trigger_data =
                                        *AggregatableTriggerDataList::Create(
                                            {*AggregatableTriggerData::Create(
                                                 /*key_piece=*/1,
                                                 /*source_keys=*/{"a"},
                                                 /*filters=*/Filters(),
                                                 /*not_filters=*/Filters()),
                                             *AggregatableTriggerData::Create(
                                                 /*key_piece=*/2,
                                                 /*source_keys=*/{"b"},
                                                 /*filters=*/Filters(),
                                                 /*not_filters=*/Filters())});
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
          TriggerRegistrationWith(
              reporting_origin,
              [](auto& r) {
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
          TriggerRegistrationWith(reporting_origin,
                                  [](auto& r) { r.debug_reporting = true; }),
      },
      {
          "debug_reporting_wrong_type",
          R"json({"debug_reporting":"true"})json",
          TriggerRegistration(reporting_origin),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_EQ(TriggerRegistration::Parse(std::move(value.GetDict()),
                                         reporting_origin),
              test_case.expected)
        << test_case.description;
  }
}

TEST(TriggerRegistrationTest, Parse_EventTriggerDataCount) {
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://r.example");

  const auto parse_with_event_triggers = [&](size_t n) {
    base::Value::List list;
    for (size_t i = 0; i < n; ++i) {
      list.Append(base::Value::Dict());
    }

    base::Value::Dict dict;
    dict.Set("event_trigger_data", std::move(list));
    return TriggerRegistration::Parse(std::move(dict), reporting_origin);
  };

  for (size_t count = 0; count <= kMaxEventTriggerData; ++count) {
    EXPECT_TRUE(parse_with_event_triggers(count).has_value());
  }

  EXPECT_EQ(
      parse_with_event_triggers(kMaxEventTriggerData + 1),
      base::unexpected(TriggerRegistrationError::kEventTriggerDataListTooLong));
}

TEST(TriggerRegistrationTest, Parse_AggregatableTriggerDataCount) {
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://r.example");

  const auto parse_with_aggregatable_trigger_data = [&](size_t n) {
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
    return TriggerRegistration::Parse(std::move(dict), reporting_origin);
  };

  for (size_t count = 0; count <= kMaxAggregatableTriggerDataPerTrigger;
       ++count) {
    EXPECT_TRUE(parse_with_aggregatable_trigger_data(count).has_value());
  }

  EXPECT_EQ(parse_with_aggregatable_trigger_data(
                kMaxAggregatableTriggerDataPerTrigger + 1),
            base::unexpected(
                TriggerRegistrationError::kAggregatableTriggerDataListTooLong));
}

}  // namespace
}  // namespace attribution_reporting
