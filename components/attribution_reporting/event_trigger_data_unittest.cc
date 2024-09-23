// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_trigger_data.h"

#include <stdint.h>

#include <limits>

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

TEST(EventTriggerDataTest, FromJSON) {
  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<
        base::expected<EventTriggerData, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(AllOf(Field(&EventTriggerData::data, 0),
                        Field(&EventTriggerData::priority, 0),
                        Field(&EventTriggerData::dedup_key, std::nullopt),
                        Field(&EventTriggerData::filters, FilterPair()))),
      },
      {
          "trigger_data_valid",
          R"json({"trigger_data":"123"})json",
          ValueIs(Field(&EventTriggerData::data, 123)),
      },
      {
          "trigger_data_wrong_type",
          R"json({"trigger_data":123})json",
          ErrorIs(TriggerRegistrationError::kEventTriggerDataValueInvalid),
      },
      {
          "trigger_data_invalid",
          R"json({"trigger_data":"-5"})json",
          ErrorIs(TriggerRegistrationError::kEventTriggerDataValueInvalid),
      },
      {
          "priority_valid",
          R"json({"priority":"-5"})json",
          ValueIs(Field(&EventTriggerData::priority, -5)),
      },
      {
          "priority_wrong_type",
          R"json({"priority":123})json",
          ErrorIs(TriggerRegistrationError::kEventPriorityValueInvalid),
      },
      {
          "priority_invalid",
          R"json({"priority":"abc"})json",
          ErrorIs(TriggerRegistrationError::kEventPriorityValueInvalid),
      },
      {
          "dedup_key_valid",
          R"json({"deduplication_key":"3"})json",
          ValueIs(Field(&EventTriggerData::dedup_key, 3)),
      },
      {
          "dedup_key_wrong_type",
          R"json({"deduplication_key":123})json",
          ErrorIs(TriggerRegistrationError::kEventDedupKeyValueInvalid),
      },
      {
          "dedup_key_invalid",
          R"json({"deduplication_key":"abc"})json",
          ErrorIs(TriggerRegistrationError::kEventDedupKeyValueInvalid),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"], "_lookback_window": 1}})json",
          ValueIs(Field(
              &EventTriggerData::filters,
              FilterPair(
                  /*positive=*/{*FilterConfig::Create(
                      {{"a", {"b"}}}, /*lookback_window=*/base::Seconds(1))},
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
          ValueIs(Field(&EventTriggerData::filters,
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
    EXPECT_THAT(EventTriggerData::FromJSON(value), test_case.matches);
  }
}

TEST(EventTriggerDataTest, ToJson) {
  const struct {
    EventTriggerData input;
    const char* expected_json;
  } kTestCases[] = {
      {
          EventTriggerData(),
          R"json({
            "trigger_data": "0",
            "priority": "0",
          })json",
      },
      {
          EventTriggerData(
              /*data=*/1,
              /*priority=*/-2,
              /*dedup_key=*/3,
              FilterPair(
                  /*positive=*/{*FilterConfig::Create(
                      {{"a", {}}}, /*lookback_window=*/base::Seconds(2))},
                  /*negative=*/{*FilterConfig::Create({{"b", {}}})})),
          R"json({
            "trigger_data": "1",
            "priority": "-2",
            "deduplication_key": "3",
            "filters": [{"a": [], "_lookback_window": 2 }],
            "not_filters": [{"b": []}]
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(EventTriggerValueTest, Constructor) {
  EXPECT_EQ(EventTriggerValue(), 1u);
  EXPECT_EQ(EventTriggerValue(5), 5u);
  EXPECT_DEATH_IF_SUPPORTED(EventTriggerValue(0), "");
}

TEST(EventTriggerValueTest, Parse) {
  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<
        base::expected<EventTriggerValue, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(1),
      },
      {
          "wrong_type",
          R"json({"value":null})json",
          ErrorIs(TriggerRegistrationError::kEventValueInvalid),
      },
      {
          "zero",
          R"json({"value":0})json",
          ErrorIs(TriggerRegistrationError::kEventValueInvalid),
      },
      {
          "negative",
          R"json({"value":-1})json",
          ErrorIs(TriggerRegistrationError::kEventValueInvalid),
      },
      {
          "fractional",
          R"json({"value":1.5})json",
          ErrorIs(TriggerRegistrationError::kEventValueInvalid),
      },
      {
          "maximal",
          R"json({"value":4294967295})json",
          ValueIs(std::numeric_limits<uint32_t>::max()),
      },
      {
          "too_large",
          R"json({"value":4294967296})json",
          ErrorIs(TriggerRegistrationError::kEventValueInvalid),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(EventTriggerValue::Parse(dict), test_case.matches);
  }
}

TEST(EventTriggerValueTest, Serialize) {
  {
    base::Value::Dict dict;
    EventTriggerValue(5).Serialize(dict);
    EXPECT_THAT(dict, base::test::IsJson(R"json({"value": 5})json"));
  }

  {
    base::Value::Dict dict;
    EventTriggerValue(std::numeric_limits<uint32_t>::max()).Serialize(dict);
    EXPECT_THAT(dict, base::test::IsJson(R"json({"value": 4294967295})json"));
  }
}

}  // namespace
}  // namespace attribution_reporting
