// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_trigger_data.h"

#include "base/functional/function_ref.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

EventTriggerData EventTriggerDataWith(
    base::FunctionRef<void(EventTriggerData&)> f) {
  EventTriggerData data;
  f(data);
  return data;
}

TEST(EventTriggerDataTest, FromJSON) {
  const struct {
    const char* description;
    const char* json;
    base::expected<EventTriggerData, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          EventTriggerData(),
      },
      {
          "trigger_data_valid",
          R"json({"trigger_data":"123"})json",
          EventTriggerDataWith([](EventTriggerData& data) { data.data = 123; }),
      },
      {
          "trigger_data_wrong_type",
          R"json({"trigger_data":123})json",
          EventTriggerData(),
      },
      {
          "trigger_data_invalid",
          R"json({"trigger_data":"-5"})json",
          EventTriggerData(),
      },
      {
          "priority_valid",
          R"json({"priority":"-5"})json",
          EventTriggerDataWith(
              [](EventTriggerData& data) { data.priority = -5; }),
      },
      {
          "priority_wrong_type",
          R"json({"priority":123})json",
          EventTriggerData(),
      },
      {
          "priority_invalid",
          R"json({"priority":"abc"})json",
          EventTriggerData(),
      },
      {
          "dedup_key_valid",
          R"json({"deduplication_key":"3"})json",
          EventTriggerDataWith(
              [](EventTriggerData& data) { data.dedup_key = 3; }),
      },
      {
          "dedup_key_wrong_type",
          R"json({"deduplication_key":123})json",
          EventTriggerData(),
      },
      {
          "dedup_key_invalid",
          R"json({"deduplication_key":"abc"})json",
          EventTriggerData(),
      },
      {
          "filters_valid",
          R"json({"filters":{"a":["b"]}})json",
          EventTriggerDataWith([](EventTriggerData& data) {
            data.filters.positive = *Filters::Create({{"a", {"b"}}});
          }),
      },
      {
          "filters_wrong_type",
          R"json({"filters":123})json",
          base::unexpected(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "not_filters_valid",
          R"json({"not_filters":{"a":["b"]}})json",
          EventTriggerDataWith([](EventTriggerData& data) {
            data.filters.negative = *Filters::Create({{"a", {"b"}}});
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
    EXPECT_EQ(EventTriggerData::FromJSON(value), test_case.expected)
        << test_case.description;
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
              FilterPair{.positive = *Filters::Create({{"a", {}}}),
                         .negative = *Filters::Create({{"b", {}}})}),
          R"json({
            "trigger_data": "1",
            "priority": "-2",
            "deduplication_key": "3",
            "filters": {"a": []},
            "not_filters": {"b": []}
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
