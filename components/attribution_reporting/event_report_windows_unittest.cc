// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_report_windows.h"

#include <optional>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/source_type.mojom-shared.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Property;

using WindowResult = EventReportWindows::WindowResult;

TEST(EventReportWindowsTest, FromDefaults) {
  const struct {
    const char* desc;
    base::TimeDelta report_window;
    SourceType source_type;
    ::testing::Matcher<std::optional<EventReportWindows>> matches;
  } kTestCases[] = {
      {
          "negative-navigation",
          base::Seconds(-1),
          SourceType::kNavigation,
          Eq(std::nullopt),
      },
      {
          "negative-event",
          base::Seconds(-1),
          SourceType::kEvent,
          Eq(std::nullopt),
      },
      {
          "=-last-navigation",
          base::Days(7),
          SourceType::kNavigation,
          Optional(AllOf(
              Property(&EventReportWindows::start_time, base::TimeDelta()),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Days(2), base::Days(7))))),
      },
      {
          ">-last-navigation",
          base::Days(7) + base::Seconds(1),
          SourceType::kNavigation,
          Optional(AllOf(
              Property(&EventReportWindows::start_time, base::TimeDelta()),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Days(2), base::Days(7),
                                   base::Days(7) + base::Seconds(1))))),
      },
      {
          "<-last-navigation",
          base::Days(7) - base::Seconds(1),
          SourceType::kNavigation,
          Optional(AllOf(
              Property(&EventReportWindows::start_time, base::TimeDelta()),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Days(2),
                                   base::Days(7) - base::Seconds(1))))),
      },
      {
          "<-first-navigation",
          base::Days(2) - base::Seconds(1),
          SourceType::kNavigation,
          Optional(AllOf(
              Property(&EventReportWindows::start_time, base::TimeDelta()),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Days(2) - base::Seconds(1))))),
      },
      {
          "event",
          base::Days(30),
          SourceType::kEvent,
          Optional(AllOf(
              Property(&EventReportWindows::start_time, base::TimeDelta()),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Days(30))))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    EXPECT_THAT(EventReportWindows::FromDefaults(test_case.report_window,
                                                 test_case.source_type),
                test_case.matches);
  }
}

TEST(EventReportWindowsTest, CreateWindows) {
  const struct {
    const char* name;
    base::TimeDelta start_time;
    std::vector<base::TimeDelta> end_times;
    ::testing::Matcher<std::optional<EventReportWindows>> matches;
  } kTestCases[] = {
      {
          .name = "end_time-eq-start_time",
          .start_time = base::Hours(1),
          .end_times = {base::Hours(1)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "end_time-lt-start_time",
          .start_time = base::Hours(2),
          .end_times = {base::Hours(2) - base::Microseconds(1)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "end_time-eq-prev-end_time",
          .start_time = base::Seconds(0),
          .end_times = {base::Hours(1), base::Hours(1)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "end_time-lt-prev-end_time",
          .start_time = base::Seconds(0),
          .end_times = {base::Hours(2), base::Hours(2) - base::Microseconds(1)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "negative-start_time",
          .start_time = base::Seconds(-1),
          .end_times = {base::Hours(1)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "empty-end_times",
          .start_time = base::Seconds(0),
          .end_times = {},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "too-many-end_times",
          .start_time = base::Seconds(0),
          .end_times = {base::Hours(1), base::Hours(2), base::Hours(3),
                        base::Hours(4), base::Hours(5), base::Hours(6)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "end-time-less-than-min-report-window",
          .start_time = base::Seconds(0),
          .end_times = {base::Hours(1) - base::Microseconds(1)},
          .matches = Eq(std::nullopt),
      },
      {
          .name = "valid",
          .start_time = base::Seconds(0),
          .end_times = {base::Hours(1), base::Hours(2), base::Hours(3),
                        base::Hours(4), base::Hours(5)},
          .matches = Optional(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(0)),
              Property(
                  &EventReportWindows::end_times,
                  ElementsAre(base::Hours(1), base::Hours(2), base::Hours(3),
                              base::Hours(4), base::Hours(5))))),
      },
      {
          .name = "valid-non-zero_start_time",
          .start_time = base::Seconds(1),
          .end_times = {base::Hours(2), base::Hours(3), base::Hours(4),
                        base::Hours(5), base::Hours(6)},
          .matches = Optional(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(1)),
              Property(
                  &EventReportWindows::end_times,
                  ElementsAre(base::Hours(2), base::Hours(3), base::Hours(4),
                              base::Hours(5), base::Hours(6))))),
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    auto actual =
        EventReportWindows::Create(test_case.start_time, test_case.end_times);
    EXPECT_THAT(actual, test_case.matches);
  }
}

TEST(EventReportWindowsTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<EventReportWindows, SourceRegistrationError>>
        matches;
    SourceType source_type = SourceType::kNavigation;
    base::TimeDelta expiry = base::Days(30);
  } kTestCases[] = {
      {
          "neither_field_present_navigation",
          R"json({})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Days(30),
                                                    SourceType::kNavigation)),
          SourceType::kNavigation,
      },
      {
          "neither_field_present_event",
          R"json({})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Days(30),
                                                    SourceType::kEvent)),
          SourceType::kEvent,
      },
      {
          "event_report_window_valid_navigation",
          R"json({"event_report_window":"86401"})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Seconds(86401),
                                                    SourceType::kNavigation)),
      },
      {
          "event_report_window_valid_event",
          R"json({"event_report_window":"86401"})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Seconds(86401),
                                                    SourceType::kEvent)),
      },
      {
          "event_report_window_valid_int",
          R"json({"event_report_window":86401})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Seconds(86401),
                                                    SourceType::kNavigation)),
      },
      {
          "event_report_window_valid_int_trailing_zero",
          R"json({"event_report_window": 86401.0})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Seconds(86401),
                                                    SourceType::kNavigation)),
      },
      {
          "event_report_window_wrong_type",
          R"json({"event_report_window": false})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_invalid",
          R"json({"event_report_window":"abc"})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_negative",
          R"json({"event_report_window":"-86401"})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_negative_int",
          R"json({"event_report_window":-86401})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_clamped_min",
          R"json({"event_report_window":3599})json",
          ValueIs(*EventReportWindows::FromDefaults(base::Seconds(3600),
                                                    SourceType::kNavigation)),
      },
      {
          .desc = "event_report_window_clamped_max",
          .json = R"json({"event_report_window":86401})json",
          .matches = ValueIs(*EventReportWindows::FromDefaults(
              base::Seconds(86400), SourceType::kNavigation)),
          .expiry = base::Seconds(86400),
      },
      {
          .desc = "event_report_window_clamped_gt_max_int32",
          .json = R"json({"event_report_window": 2147483648})json",
          .matches = ValueIs(*EventReportWindows::FromDefaults(
              base::Seconds(86400), SourceType::kNavigation)),
          .expiry = base::Seconds(86400),
      },
      {
          "event_report_windows_wrong_type",
          R"json({"event_report_windows":0})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowsWrongType),
      },
      {
          "event_report_windows_empty_dict",
          R"json({"event_report_windows":{}})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowsEndTimesMissing),
      },
      {
          "event_report_windows_start_time_wrong_type",
          R"json({"event_report_windows":{
            "start_time":"0",
            "end_times":[96000,172800]
          }})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowsStartTimeInvalid),
      },
      {
          "event_report_windows_start_time_not_int",
          R"json({"event_report_windows":{
            "start_time": 3600.1,
            "end_times": [96000, 172800]
          }})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowsStartTimeInvalid),
      },
      {
          "event_report_windows_start_time_negative",
          R"json({"event_report_windows":{
            "start_time":-3600,
            "end_times":[96000,172800]
          }})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowsStartTimeInvalid),
      },
      {
          .desc = "event_report_windows_start_time_gt_expiry",
          .json = R"json({"event_report_windows":{
            "start_time":86401,
            "end_times":[96000]
          }})json",
          .matches = ErrorIs(
              SourceRegistrationError::kEventReportWindowsStartTimeInvalid),
          .expiry = base::Seconds(86400),
      },
      {
          "event_report_windows_end_times_missing",
          R"json({"event_report_windows":{
            "start_time":0
          }})json",
          ErrorIs(SourceRegistrationError::kEventReportWindowsEndTimesMissing),
      },
      {
          "event_report_windows_end_times_wrong_type",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":96000
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimesListInvalid),
      },
      {
          "event_report_windows_end_times_list_empty",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[]
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimesListInvalid),
      },
      {
          "event_report_windows_end_times_list_too_long",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[3600,7200,10800,14400,18000,21600]
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimesListInvalid),
      },
      {
          "event_report_windows_end_times_value_wrong_type",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":["3600"]
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid),
      },
      {
          "event_report_windows_end_times_value_not_int",
          R"json({"event_report_windows":{
            "start_time": 0,
            "end_times": [3600.1]
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid),
      },
      {
          "event_report_windows_end_times_value_negative",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[-3600]
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid),
      },
      {
          "event_report_windows_end_times_value_zero",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[0]
          }})json",
          ErrorIs(
              SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid),
      },
      {
          "event_report_windows_start_time_equal_end",
          R"json({"event_report_windows":{
            "start_time":3600,
            "end_times":[3600]
          }})json",
          ErrorIs(SourceRegistrationError::
                      kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "event_report_windows_start_duration_equal_end",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[3600,3600]
          }})json",
          ErrorIs(SourceRegistrationError::
                      kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "event_report_windows_start_duration_greater_than_end",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[5400,3600]
          }})json",
          ErrorIs(SourceRegistrationError::
                      kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "event_report_windows_end_time_clamped_min",
          R"json({"event_report_windows":{
            "start_time":3599,
            "end_times":[3599]
          }})json",
          ValueIs(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(3599)),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Seconds(3600))))),
      },
      {
          .desc = "event_report_windows_end_time_clamped_max",
          .json = R"json({"event_report_windows":{"end_times":[86401]}})json",
          .matches = ValueIs(
              AllOf(Property(&EventReportWindows::start_time, base::Seconds(0)),
                    Property(&EventReportWindows::end_times,
                             ElementsAre(base::Seconds(86400))))),
          .expiry = base::Seconds(86400),
      },
      {
          .desc = "event_report_windows_end_times_value_gt_int_max",
          .json = R"json({"event_report_windows":{
            "start_time": 0,
            "end_times": [2147483648]
          }})json",
          .matches = ValueIs(Property(&EventReportWindows::end_times,
                                      ElementsAre(base::Seconds(86400)))),
          .expiry = base::Seconds(86400),
      },
      {
          "event_report_windows_valid",
          R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[3600,10800,21600]
          }})json",
          ValueIs(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(0)),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Seconds(3600), base::Seconds(10800),
                                   base::Seconds(21600))))),
      },
      {
          "event_report_windows_valid_trailing_zero",
          R"json({"event_report_windows":{
            "start_time": 1.0,
            "end_times": [3600.0, 10800, 21600]
          }})json",
          ValueIs(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(1)),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Seconds(3600), base::Seconds(10800),
                                   base::Seconds(21600))))),
      },
      {
          "event_report_windows_valid_start_time_missing",
          R"json({"event_report_windows":{
            "end_times":[3600,10800,21600]
          }})json",
          ValueIs(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(0)),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Seconds(3600), base::Seconds(10800),
                                   base::Seconds(21600))))),
      },
      {
          "event_report_windows_valid_start_time_set",
          R"json({"event_report_windows":{
            "start_time":7200,
            "end_times":[16000,32000,48000]
          }})json",
          ValueIs(AllOf(
              Property(&EventReportWindows::start_time, base::Seconds(7200)),
              Property(&EventReportWindows::end_times,
                       ElementsAre(base::Seconds(16000), base::Seconds(32000),
                                   base::Seconds(48000))))),
      },
      {
          "event_report_windows_valid_end_time_less_than_default",
          R"json({"event_report_windows":{
            "end_times":[1800]
          }})json",
          ValueIs(
              AllOf(Property(&EventReportWindows::start_time, base::Seconds(0)),
                    Property(&EventReportWindows::end_times,
                             ElementsAre(base::Seconds(3600))))),
      },
      {
          "both_event_report_window_fields_present",
          R"json({
            "event_report_window":"86401",
            "event_report_windows": {
              "end_times": [86401]
            },
            "destination":"https://d.example"
          })json",
          ErrorIs(SourceRegistrationError::kBothEventReportWindowFieldsFound),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    auto actual =
        EventReportWindows::FromJSON(base::test::ParseJsonDict(test_case.json),
                                     test_case.expiry, test_case.source_type);
    EXPECT_THAT(actual, test_case.matches);
  }
}

TEST(EventReportWindowsTest, ComputeReportTime) {
  const EventReportWindows kDefaultReportWindows = *EventReportWindows::Create(
      base::Hours(0), {base::Hours(2), base::Days(1), base::Days(7)});
  const base::Time kSourceTime = base::Time();

  const struct {
    base::Time trigger_time;
    base::Time expected;
  } kTestCases[] = {
      {
          .trigger_time = kSourceTime,
          .expected = kSourceTime + base::Hours(2),
      },
      {
          .trigger_time = kSourceTime - base::Seconds(1),
          .expected = kSourceTime + base::Hours(2),
      },
      {
          .trigger_time = kSourceTime + base::Hours(2) - base::Milliseconds(1),
          .expected = kSourceTime + base::Hours(2),
      },
      {
          .trigger_time = kSourceTime + base::Hours(2),
          .expected = kSourceTime + base::Days(1),
      },
      {
          .trigger_time = kSourceTime + base::Days(1) - base::Milliseconds(1),
          .expected = kSourceTime + base::Days(1),
      },
      {
          .trigger_time = kSourceTime + base::Days(1),
          .expected = kSourceTime + base::Days(7),
      },
      {
          .trigger_time = kSourceTime + base::Days(7),
          .expected = kSourceTime + base::Days(7),
      }};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(kDefaultReportWindows.ComputeReportTime(kSourceTime,
                                                      test_case.trigger_time),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, ReportTimeAtWindow) {
  const EventReportWindows kDefaultReportWindows = *EventReportWindows::Create(
      base::Hours(0), {base::Hours(1), base::Days(3), base::Days(7)});
  base::Time kSourceTime = base::Time();

  const struct {
    int index;
    base::Time expected;
  } kTestCases[] = {
      {
          .index = 0,
          .expected = kSourceTime + base::Hours(1),
      },
      {
          .index = 1,
          .expected = kSourceTime + base::Days(3),
      },
      {
          .index = 2,
          .expected = kSourceTime + base::Days(7),
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        kDefaultReportWindows.ReportTimeAtWindow(kSourceTime, test_case.index),
        test_case.expected);
  }
}

TEST(EventReportWindowsTest, StartTimeAtWindow) {
  const EventReportWindows kDefaultReportWindows = *EventReportWindows::Create(
      base::Hours(0), {base::Hours(1), base::Days(3), base::Days(7)});
  base::Time kSourceTime = base::Time();

  const struct {
    int index;
    base::Time expected;
  } kTestCases[] = {
      {
          .index = 0,
          .expected = kSourceTime,
      },
      {
          .index = 1,
          .expected = kSourceTime + base::Hours(1),
      },
      {
          .index = 2,
          .expected = kSourceTime + base::Days(3),
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        kDefaultReportWindows.StartTimeAtWindow(kSourceTime, test_case.index),
        test_case.expected);
  }
}

TEST(EventReportWindowsTest, FallsWithin) {
  const EventReportWindows kDefaultReportWindows =
      *EventReportWindows::Create(base::Hours(1), {base::Hours(2)});
  const EventReportWindows kDefaultReportWindowsNoStartTime =
      *EventReportWindows::Create(base::Hours(0), {base::Hours(2)});

  const struct {
    EventReportWindows report_windows;
    base::TimeDelta trigger_moment;
    WindowResult expected;
  } kTestCases[] = {
      {
          .report_windows = kDefaultReportWindows,
          .trigger_moment = base::Hours(0),
          .expected = WindowResult::kNotStarted,
      },
      {
          .report_windows = kDefaultReportWindows,
          .trigger_moment = base::Hours(1) - base::Milliseconds(1),
          .expected = WindowResult::kNotStarted,
      },
      {
          .report_windows = kDefaultReportWindows,
          .trigger_moment = base::Hours(1),
          .expected = WindowResult::kFallsWithin,
      },
      {
          .report_windows = kDefaultReportWindows,
          .trigger_moment = base::Hours(2) - base::Milliseconds(1),
          .expected = WindowResult::kFallsWithin,
      },
      {
          .report_windows = kDefaultReportWindows,
          .trigger_moment = base::Hours(2),
          .expected = WindowResult::kPassed,
      },
      // TODO(crbug.com/40283992): Remove case once DCHECK is used in
      // implementation.
      {
          .report_windows = kDefaultReportWindowsNoStartTime,
          .trigger_moment = base::Hours(-1),
          .expected = WindowResult::kFallsWithin,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.report_windows.FallsWithin(test_case.trigger_moment),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, Serialize) {
  const struct {
    EventReportWindows input;
    const char* expected;
  } kTestCases[] = {
      {
          *EventReportWindows::Create(base::Seconds(0),
                                      {base::Days(1), base::Days(5)}),
          R"json({"event_report_windows": {
            "start_time":0,
            "end_times":[86400,432000]
          }})json",
      },
      {
          *EventReportWindows::Create(base::Hours(1),
                                      {base::Days(1), base::Days(5)}),
          R"json({"event_report_windows": {
            "start_time":3600,
            "end_times":[86400,432000]
          }})json",
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value::Dict actual;
    test_case.input.Serialize(actual);
    EXPECT_THAT(actual, base::test::IsJson(test_case.expected));
  }
}

}  // namespace
}  // namespace attribution_reporting
