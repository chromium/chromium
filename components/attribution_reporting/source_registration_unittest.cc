// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <utility>

#include "base/functional/function_ref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::Field;

SourceRegistration SourceRegistrationWith(
    DestinationSet destination_set,
    base::FunctionRef<void(SourceRegistration&)> f) {
  SourceRegistration r(std::move(destination_set));
  f(r);
  return r;
}

TEST(SourceRegistrationTest, Parse) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});

  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<SourceRegistration, SourceRegistrationError>>
        matches;
    SourceType source_type = SourceType::kNavigation;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          ErrorIs(SourceRegistrationError::kInvalidJson),
      },
      {
          "root_wrong_type",
          "3",
          ErrorIs(SourceRegistrationError::kRootWrongType),
      },
      {
          "required_fields_only",
          R"json({"destination":"https://d.example"})json",
          ValueIs(AllOf(
              Field(&SourceRegistration::source_event_id, 0),
              Field(&SourceRegistration::destination_set, destination),
              Field(&SourceRegistration::expiry, base::Days(30)),
              Field(&SourceRegistration::event_report_windows, absl::nullopt),
              Field(&SourceRegistration::aggregatable_report_window,
                    base::Days(30)),
              Field(&SourceRegistration::max_event_level_reports, 3),
              Field(&SourceRegistration::priority, 0),
              Field(&SourceRegistration::filter_data, FilterData()),
              Field(&SourceRegistration::debug_key, absl::nullopt),
              Field(&SourceRegistration::aggregation_keys, AggregationKeys()),
              Field(&SourceRegistration::debug_reporting, false))),
      },
      {
          "source_event_id_valid",
          R"json({"source_event_id":"1","destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::source_event_id, 1)),
      },
      {
          "source_event_id_wrong_type",
          R"json({"source_event_id":1,"destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kSourceEventIdValueInvalid),
      },
      {
          "source_event_id_invalid",
          R"json({"source_event_id":"-1","destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kSourceEventIdValueInvalid),
      },
      {
          "destination_missing",
          R"json({})json",
          ErrorIs(SourceRegistrationError::kDestinationMissing),
      },
      {
          "priority_valid",
          R"json({"priority":"-5","destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::priority, -5)),
      },
      {
          "priority_wrong_type",
          R"json({"priority":-5,"destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kPriorityValueInvalid),
      },
      {
          "priority_invalid",
          R"json({"priority":"abc","destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kPriorityValueInvalid),
      },
      {
          "expiry_valid",
          R"json({"expiry":"172801","destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Seconds(172801))),
      },
      {
          "expiry_valid_int",
          R"json({"expiry":172800,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Seconds(172800))),
      },
      {
          "expiry_wrong_type",
          R"json({"expiry":1728000.1,"destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kExpiryValueInvalid),
      },
      {
          "expiry_invalid",
          R"json({"expiry":"abc","destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kExpiryValueInvalid),
      },
      {
          "expiry_negative",
          R"json({"expiry":"-172801","destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kExpiryValueInvalid),
      },
      {
          "expiry_negative_int",
          R"json({"expiry":-172801,"destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kExpiryValueInvalid),
      },
      {
          "expiry_clamped_min",
          R"json({"expiry":86399,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Days(1))),
      },
      {
          "expiry_clamped_max",
          R"json({"expiry":2592001,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Days(30))),
      },
      {
          "expiry_not_rounded_to_whole_day",
          R"json({"expiry":86401,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Seconds(86401))),
          SourceType::kNavigation,
      },
      {
          "expiry_rounded_to_whole_day_down",
          R"json({"expiry":86401,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Days(1))),
          SourceType::kEvent,
      },
      {
          "expiry_rounded_to_whole_day_up",
          R"json({"expiry":172799,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::expiry, base::Days(2))),
          SourceType::kEvent,
      },
      {
          "event_report_window_valid",
          R"json({"event_report_window":"86401",
          "destination":"https://d.example"})json",
          ValueIs(Field(
              &SourceRegistration::event_report_windows,
              *EventReportWindows::CreateSingularWindow(base::Seconds(86401)))),
      },
      {
          "event_report_windows_valid",
          R"json({
            "event_report_windows": {
              "end_times": [86401]
            },
            "destination":"https://d.example"
          })json",
          ValueIs(Field(&SourceRegistration::event_report_windows,
                        *EventReportWindows::CreateWindows(
                            base::Seconds(0), {base::Seconds(86401)}))),

      },
      {
          "aggregatable_report_window_valid",
          R"json({"aggregatable_report_window":"86401",
          "destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::aggregatable_report_window,
                        base::Seconds(86401))),
      },
      {
          "aggregatable_report_window_valid_int",
          R"json({"aggregatable_report_window":86401,
          "destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::aggregatable_report_window,
                        base::Seconds(86401))),
      },
      {
          "aggregatable_report_window_wrong_type",
          R"json({"aggregatable_report_window":86401.1,
          "destination":"https://d.example"})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableReportWindowValueInvalid),
      },
      {
          "aggregatable_report_window_invalid",
          R"json({"aggregatable_report_window":"abc",
          "destination":"https://d.example"})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableReportWindowValueInvalid),
      },
      {
          "aggregatable_report_window_negative",
          R"json({"aggregatable_report_window":"-86401",
          "destination":"https://d.example"})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableReportWindowValueInvalid),
      },
      {
          "aggregatable_report_window_negative_int",
          R"json({"aggregatable_report_window":-86401,
          "destination":"https://d.example"})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableReportWindowValueInvalid),
      },
      {
          "aggregatable_report_window_clamped_min",
          R"json({"aggregatable_report_window":3599,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::aggregatable_report_window,
                        base::Seconds(3600))),
      },
      {
          "aggregatable_report_window_clamped_max",
          R"json({"aggregatable_report_window":259200,"expiry":172800,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::aggregatable_report_window,
                        base::Seconds(172800))),
      },
      {
          "max_event_level_reports_omitted_event",
          R"json({"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::max_event_level_reports, 1)),
          SourceType::kEvent,
      },
      {
          "max_event_level_reports_valid",
          R"json({"max_event_level_reports":5,
          "destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::max_event_level_reports, 5)),
      },
      {
          "max_event_level_reports_wrong_type",
          R"json({"max_event_level_reports":"5",
          "destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "max_event_level_reports_negative",
          R"json({"max_event_level_reports":-5,
          "destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "max_event_level_reports_zero",
          R"json({"max_event_level_reports":0,
          "destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::max_event_level_reports, 0)),
      },
      {
          "max_event_level_reports_higher_than_max",
          R"json({"max_event_level_reports":25,
          "destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "debug_key_valid",
          R"json({"debug_key":"5","destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::debug_key, 5)),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5","destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::debug_key, absl::nullopt)),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::debug_key, absl::nullopt)),
      },
      {
          "filter_data_valid",
          R"json({"filter_data":{"a":["b"]},"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::filter_data,
                        *FilterData::Create({{"a", {"b"}}}))),
      },
      {
          "filter_data_wrong_type",
          R"json({"filter_data":5,"destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kFilterDataWrongType),
      },
      {
          "aggregation_keys_valid",
          R"json({"aggregation_keys":{"a":"0x1"},"destination":"https://d.example"})json",
          ValueIs(Field(
              &SourceRegistration::aggregation_keys,
              *AggregationKeys::FromKeys({{"a", absl::MakeUint128(0, 1)}}))),
      },
      {
          "aggregation_keys_wrong_type",
          R"json({"aggregation_keys":5,"destination":"https://d.example"})json",
          ErrorIs(SourceRegistrationError::kAggregationKeysWrongType),
      },
      {
          "debug_reporting_valid",
          R"json({"debug_reporting":true,"destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::debug_reporting, true)),
      },
      {
          "debug_reporting_wrong_type",
          R"json({"debug_reporting":"true","destination":"https://d.example"})json",
          ValueIs(Field(&SourceRegistration::debug_reporting, false)),
      },
  };

  static constexpr char kSourceRegistrationErrorMetric[] =
      "Conversions.SourceRegistrationError5";

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::HistogramTester histograms;

    auto source =
        SourceRegistration::Parse(test_case.json, test_case.source_type);
    EXPECT_THAT(source, test_case.matches);

    if (source.has_value()) {
      histograms.ExpectTotalCount(kSourceRegistrationErrorMetric, 0);
    } else {
      histograms.ExpectUniqueSample(kSourceRegistrationErrorMetric,
                                    source.error(), 1);
    }
  }
}

TEST(SourceRegistrationTest, ToJson) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});

  const struct {
    SourceRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          SourceRegistration(destination),
          R"json({
            "aggregatable_report_window": 2592000,
            "debug_reporting": false,
            "destination":"https://d.example",
            "expiry": 2592000,
            "max_event_level_reports": 0,
            "priority": "0",
            "source_event_id": "0"
          })json",
      },
      {
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) {
                r.aggregatable_report_window = base::Seconds(1);
                r.aggregation_keys = *AggregationKeys::FromKeys({{"a", 2}});
                r.debug_key = 3;
                r.debug_reporting = true;
                r.event_report_windows =
                    *EventReportWindows::CreateSingularWindow(base::Seconds(4));
                r.expiry = base::Seconds(5);
                r.filter_data = *FilterData::Create({{"b", {}}});
                r.priority = -6;
                r.source_event_id = 7;
                r.max_event_level_reports = 8;
              }),
          R"json({
            "aggregatable_report_window": 1,
            "aggregation_keys": {"a": "0x2"},
            "debug_key": "3",
            "debug_reporting": true,
            "destination":"https://d.example",
            "event_report_window": 4,
            "expiry": 5,
            "filter_data": {"b": []},
            "priority": "-6",
            "source_event_id": "7",
            "max_event_level_reports": 8,
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(SourceRegistrationTest, IsValid) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});

  EXPECT_TRUE(SourceRegistration(destination).IsValid());

  EXPECT_FALSE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                 r.expiry = base::Days(1) - base::Microseconds(1);
                 r.aggregatable_report_window = r.expiry;
               }).IsValid());

  EXPECT_FALSE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                 r.expiry = base::Days(30) + base::Microseconds(1);
                 r.aggregatable_report_window = r.expiry;
               }).IsValid());

  EXPECT_TRUE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                r.expiry = base::Days(1);
                r.aggregatable_report_window = r.expiry;
              }).IsValid());

  EXPECT_TRUE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                r.expiry = base::Days(30);
                r.aggregatable_report_window = r.expiry;
              }).IsValid());

  EXPECT_FALSE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                 r.aggregatable_report_window =
                     base::Hours(1) - base::Microseconds(1);
               }).IsValid());

  EXPECT_FALSE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                 r.expiry = base::Days(1);
                 r.aggregatable_report_window =
                     r.expiry + base::Microseconds(1);
               }).IsValid());

  EXPECT_TRUE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                r.aggregatable_report_window = base::Hours(1);
              }).IsValid());

  EXPECT_FALSE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                 r.max_event_level_reports = -1;
               }).IsValid());

  EXPECT_FALSE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                 r.max_event_level_reports = 21;
               }).IsValid());

  EXPECT_TRUE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                r.max_event_level_reports = 0;
              }).IsValid());

  EXPECT_TRUE(SourceRegistrationWith(destination, [](SourceRegistration& r) {
                r.max_event_level_reports = 20;
              }).IsValid());
}

TEST(SourceRegistrationTest, IsValidForSourceType) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});

  SourceRegistration reg(destination);

  EXPECT_TRUE(reg.IsValidForSourceType(SourceType::kNavigation));
  EXPECT_TRUE(reg.IsValidForSourceType(SourceType::kEvent));

  reg.expiry -= base::Microseconds(1);
  EXPECT_TRUE(reg.IsValidForSourceType(SourceType::kNavigation));
  EXPECT_FALSE(reg.IsValidForSourceType(SourceType::kEvent));
}

}  // namespace
}  // namespace attribution_reporting
