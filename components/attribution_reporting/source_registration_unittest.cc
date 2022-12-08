// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <utility>

#include "base/functional/invoke.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

template <typename F>
SourceRegistration SourceRegistrationWith(SuitableOrigin destination, F&& f) {
  SourceRegistration r(std::move(destination));
  base::invoke<F, SourceRegistration&>(std::move(f), r);
  return r;
}

TEST(SourceRegistrationTest, Parse) {
  const auto destination_origin =
      *SuitableOrigin::Deserialize("https://d.example");

  const struct {
    const char* desc;
    const char* json;
    base::expected<SourceRegistration, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          base::unexpected(SourceRegistrationError::kInvalidJson),
      },
      {
          "root_wrong_type",
          "3",
          base::unexpected(SourceRegistrationError::kRootWrongType),
      },
      {
          "required_fields_only",
          R"json({"destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "source_event_id_valid",
          R"json({"source_event_id":"1","destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) { r.source_event_id = 1; }),
      },
      {
          "source_event_id_wrong_type",
          R"json({"source_event_id":1,"destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "source_event_id_invalid_defaults_to_0",
          R"json({"source_event_id":"-1","destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "destination_missing",
          R"json({})json",
          base::unexpected(SourceRegistrationError::kDestinationMissing),
      },
      {
          "destination_wrong_type",
          R"json({"destination":0})json",
          base::unexpected(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_untrustworthy",
          R"json({"destination":"http://d.example"})json",
          base::unexpected(SourceRegistrationError::kDestinationUntrustworthy),
      },
      {
          "priority_valid",
          R"json({"priority":"-5","destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) { r.priority = -5; }),
      },
      {
          "priority_wrong_type_defaults_to_0",
          R"json({"priority":-5,"destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "priority_invalid_defaults_to_0",
          R"json({"priority":"abc","destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "expiry_valid",
          R"json({"expiry":"172801","destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "expiry_wrong_type",
          R"json({"expiry":172800,"destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "expiry_invalid",
          R"json({"expiry":"abc","destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "event_report_window_valid",
          R"json({"expiry":"172801","event_report_window":"86401",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) {
                                   r.expiry = base::Seconds(172801);
                                   r.event_report_window = base::Seconds(86401);
                                 }),
      },
      {
          "event_report_window_wrong_type",
          R"json({"expiry":"172801","event_report_window":86401,
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "event_report_window_invalid",
          R"json({"expiry":"172801","event_report_window":"abc",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "aggregatable_report_window_valid",
          R"json({"expiry":"172801","aggregatable_report_window":"86401",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) {
                                   r.expiry = base::Seconds(172801);
                                   r.aggregatable_report_window =
                                       base::Seconds(86401);
                                 }),
      },
      {
          "aggregatable_report_window_wrong_type",
          R"json({"expiry":"172801","aggregatable_report_window":86401,
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "aggregatable_report_window_invalid",
          R"json({"expiry":"172801","aggregatable_report_window":"abc",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "debug_key_valid",
          R"json({"debug_key":"5","destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) { r.debug_key = 5; }),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5","destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5,"destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
      {
          "filter_data_valid",
          R"json({"filter_data":{"a":["b"]},"destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) {
                r.filter_data = *FilterData::Create({{"a", {"b"}}});
              }),
      },
      {
          "filter_data_wrong_type",
          R"json({"filter_data":5,"destination":"https://d.example"})json",
          base::unexpected(SourceRegistrationError::kFilterDataWrongType),
      },
      {
          "aggregation_keys_valid",
          R"json({"aggregation_keys":{"a":"0x1"},"destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) {
                                   r.aggregation_keys =
                                       *AggregationKeys::FromKeys(
                                           {{"a", absl::MakeUint128(0, 1)}});
                                 }),
      },
      {
          "aggregation_keys_wrong_type",
          R"json({"aggregation_keys":5,"destination":"https://d.example"})json",
          base::unexpected(SourceRegistrationError::kAggregationKeysWrongType),
      },
      {
          "debug_reporting_valid",
          R"json({"debug_reporting":true,"destination":"https://d.example"})json",
          SourceRegistrationWith(destination_origin,
                                 [](auto& r) { r.debug_reporting = true; }),
      },
      {
          "debug_reporting_wrong_type",
          R"json({"debug_reporting":"true","destination":"https://d.example"})json",
          SourceRegistration(destination_origin),
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, SourceRegistration::Parse(test_case.json))
        << test_case.desc;
  }
}

TEST(SourceRegistrationTest, ToJson) {
  const auto destination_origin =
      *SuitableOrigin::Deserialize("https://d.example");

  const struct {
    SourceRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          SourceRegistration(destination_origin),
          R"json({
            "debug_reporting": false,
            "destination":"https://d.example",
            "priority": "0",
            "source_event_id": "0"
          })json",
      },
      {
          SourceRegistrationWith(
              destination_origin,
              [](auto& r) {
                r.aggregatable_report_window = base::Seconds(1);
                r.aggregation_keys = *AggregationKeys::FromKeys({{"a", 2}});
                r.debug_key = 3;
                r.debug_reporting = true;
                r.event_report_window = base::Seconds(4);
                r.expiry = base::Seconds(5);
                r.filter_data = *FilterData::Create({{"b", {}}});
                r.priority = -6;
                r.source_event_id = 7;
              }),
          R"json({
            "aggregatable_report_window": "1",
            "aggregation_keys": {"a": "0x2"},
            "debug_key": "3",
            "debug_reporting": true,
            "destination":"https://d.example",
            "event_report_window": "4",
            "expiry": "5",
            "filter_data": {"b": []},
            "priority": "-6",
            "source_event_id": "7",
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
