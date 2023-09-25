// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_report_windows.h"

#include <vector>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using WindowResult = EventReportWindows::WindowResult;

TEST(EventReportWindowsTest, CreateWindow) {
  const struct {
    base::TimeDelta window_time;
    absl::optional<EventReportWindows> expected;
  } kTestCases[] = {
      {.window_time = base::Seconds(-1), .expected = absl::nullopt},
      {.window_time = base::Seconds(0),
       .expected = EventReportWindows::CreateSingularWindow(base::Seconds(0))},
  };
  for (const auto& test_case : kTestCases) {
    auto windows =
        EventReportWindows::CreateSingularWindow(test_case.window_time);
    EXPECT_EQ(windows, test_case.expected);
  }
}

TEST(EventReportWindowsTest, CreateWindows) {
  const struct {
    base::TimeDelta start_time;
    std::vector<base::TimeDelta> end_times;
    absl::optional<EventReportWindows> expected;
  } kTestCases[] = {
      {.start_time = base::Seconds(0),
       .end_times = {base::Seconds(0), base::Seconds(1)},
       .expected = absl::nullopt},
      {.start_time = base::Seconds(-1),
       .end_times = {base::Seconds(1)},
       .expected = absl::nullopt},
      {.start_time = base::Seconds(0),
       .end_times = {},
       .expected = absl::nullopt},
      {
          .start_time = base::Seconds(0),
          .end_times = {base::Seconds(1), base::Seconds(2)},
          .expected = EventReportWindows::CreateWindows(
              base::Seconds(0), {base::Seconds(1), base::Seconds(2)}),
      },
  };
  for (const auto& test_case : kTestCases) {
    auto windows = EventReportWindows::CreateWindows(test_case.start_time,
                                                     test_case.end_times);
    EXPECT_EQ(windows, test_case.expected);
  }
}

TEST(EventReportWindowsTest, CreateWindowsAndTruncate) {
  base::TimeDelta start_time = base::Seconds(5);
  std::vector<base::TimeDelta> end_times = {base::Seconds(10),
                                            base::Seconds(30)};

  const struct {
    base::TimeDelta expiry;
    absl::optional<EventReportWindows> expected;
  } kTestCases[] = {
      {.expiry = base::Seconds(5), .expected = absl::nullopt},
      {.expiry = base::Seconds(6),
       .expected =
           EventReportWindows::CreateWindows(start_time, {base::Seconds(6)})},
      {.expiry = base::Seconds(10),
       .expected =
           EventReportWindows::CreateWindows(start_time, {base::Seconds(10)})},
      {.expiry = base::Seconds(11),
       .expected = EventReportWindows::CreateWindows(
           start_time, {base::Seconds(10), base::Seconds(11)})},
      {.expiry = base::Seconds(31),
       .expected = EventReportWindows::CreateWindows(
           start_time,
           {base::Seconds(10), base::Seconds(30), base::Seconds(31)})},
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(EventReportWindows::CreateWindowsAndTruncate(
                  start_time, end_times, test_case.expiry),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, Parse) {
  const struct {
    const char* desc;
    base::Value::Dict json;
    base::expected<absl::optional<EventReportWindows>, SourceRegistrationError>
        expected;
  } kTestCases[] = {
      {"neither_field_present", base::test::ParseJsonDict(R"json({})json"),
       absl::nullopt},
      {"event_report_window_valid",
       base::test::ParseJsonDict(R"json({"event_report_window":"86401"})json"),
       EventReportWindows::CreateSingularWindow(base::Seconds(86401))},
      {"event_report_window_valid_int",
       base::test::ParseJsonDict(R"json({"event_report_window":86401})json"),
       EventReportWindows::CreateSingularWindow(base::Seconds(86401))},
      {
          "event_report_window_wrong_type",
          base::test::ParseJsonDict(
              R"json({"event_report_window":86401.1})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_invalid",
          base::test::ParseJsonDict(R"json({"event_report_window":"abc"})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_negative",
          base::test::ParseJsonDict(
              R"json({"event_report_window":"-86401"})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_window_negative_int",
          base::test::ParseJsonDict(
              R"json({"event_report_window":-86401})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowValueInvalid),
      },
      {
          "event_report_windows_wrong_type",
          base::test::ParseJsonDict(R"json({"event_report_windows":0})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsWrongType),
      },
      {
          "event_report_windows_empty_dict",
          base::test::ParseJsonDict(R"json({"event_report_windows":{}})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesMissing),
      },
      {
          "event_report_windows_start_time_wrong_type",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":"0",
            "end_times":[96000,172800]
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsStartTimeWrongType),
      },
      {
          "event_report_windows_start_time_negative",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":-3600,
            "end_times":[96000,172800]
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsStartTimeInvalid),
      },
      {
          "event_report_windows_end_times_missing",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesMissing),
      },
      {
          "event_report_windows_end_times_wrong_type",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":96000
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesWrongType),
      },
      {
          "event_report_windows_end_times_list_empty",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[]
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesListEmpty),
      },
      {
          "event_report_windows_end_times_list_too_long",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[3600,7200,10800,14400,18000,21600]
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesListTooLong),
      },
      {
          "event_report_windows_end_times_value_wrong_type",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":["3600"]
          }})json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeValueWrongType),
      },
      {
          "event_report_windows_end_times_value_negative",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[-3600]
          }})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid),
      },
      {
          "event_report_windows_start_time_equal_end",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":3600,
            "end_times":[3600]
          }})json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "event_report_windows_start_duration_equal_end",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[3600,3600]
          }})json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "event_report_windows_start_duration_greater_than_end",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[5400,3600]
          }})json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "event_report_windows_valid",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":0,
            "end_times":[3600,10800,21600]
          }})json"),
          EventReportWindows::CreateWindows(
              base::Seconds(0), {base::Seconds(3600), base::Seconds(10800),
                                 base::Seconds(21600)}),
      },
      {
          "event_report_windows_valid_start_time_missing",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "end_times":[3600,10800,21600]
          }})json"),
          EventReportWindows::CreateWindows(
              base::Seconds(0), {base::Seconds(3600), base::Seconds(10800),
                                 base::Seconds(21600)}),
      },
      {
          "event_report_windows_valid_start_time_set",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "start_time":7200,
            "end_times":[16000,32000,48000]
          }})json"),
          EventReportWindows::CreateWindows(
              base::Seconds(7200), {base::Seconds(16000), base::Seconds(32000),
                                    base::Seconds(48000)}),
      },
      {
          "event_report_windows_valid_end_time_less_than_default",
          base::test::ParseJsonDict(R"json({"event_report_windows":{
            "end_times":[1800]
          }})json"),
          EventReportWindows::CreateWindows(base::Seconds(0),
                                            {base::Seconds(3600)}),
      },
      {
          "both_event_report_window_fields_present",
          base::test::ParseJsonDict(R"json({
            "event_report_window":"86401",
            "event_report_windows": {
              "end_times": [86401]
            },
            "destination":"https://d.example"
          })json"),
          base::unexpected(
              SourceRegistrationError::kBothEventReportWindowFieldsFound),
      },
  };

  for (const auto& test_case : kTestCases) {
    auto event_report_windows = EventReportWindows::FromJSON(test_case.json);
    EXPECT_EQ(test_case.expected, event_report_windows) << test_case.desc;
  }
}

TEST(EventReportWindowsTest, ComputeReportTime) {
  const EventReportWindows kDefaultReportWindows =
      *EventReportWindows::CreateWindows(
          base::Hours(0), {base::Hours(2), base::Days(1), base::Days(7)});
  base::Time source_time = base::Time();
  const struct {
    base::Time trigger_time;
    base::Time expected;
  } kTestCases[] = {
      {
          .trigger_time = source_time,
          .expected = source_time + base::Hours(2),
      },
      {
          .trigger_time = source_time + base::Hours(2) - base::Milliseconds(1),
          .expected = source_time + base::Hours(2),
      },
      {
          .trigger_time = source_time + base::Hours(2),
          .expected = source_time + base::Days(1),
      },
      {
          .trigger_time = source_time + base::Days(1) - base::Milliseconds(1),
          .expected = source_time + base::Days(1),
      },
      {
          .trigger_time = source_time + base::Days(1),
          .expected = source_time + base::Days(7),
      },
      {
          .trigger_time = source_time + base::Days(7),
          .expected = source_time + base::Days(7),
      }};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(kDefaultReportWindows.ComputeReportTime(source_time,
                                                      test_case.trigger_time),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, ReportTimeAtWindow) {
  const EventReportWindows kDefaultReportWindows =
      *EventReportWindows::CreateWindows(
          base::Hours(0), {base::Hours(1), base::Days(3), base::Days(7)});
  base::Time source_time = base::Time();
  const struct {
    int index;
    base::Time expected;
  } kTestCases[] = {{
                        .index = 0,
                        .expected = source_time + base::Hours(1),
                    },
                    {
                        .index = 1,
                        .expected = source_time + base::Days(3),
                    },
                    {
                        .index = 2,
                        .expected = source_time + base::Days(7),
                    }};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        kDefaultReportWindows.ReportTimeAtWindow(source_time, test_case.index),
        test_case.expected);
  }
}

TEST(EventReportWindowsTest, FallsWithin) {
  const EventReportWindows kDefaultReportWindows =
      *EventReportWindows::CreateWindows(base::Hours(1), {base::Hours(2)});
  const struct {
    base::TimeDelta trigger_moment;
    WindowResult expected;
  } kTestCases[] = {
      {
          .trigger_moment = base::Hours(0),
          .expected = WindowResult::kNotStarted,
      },
      {
          .trigger_moment = base::Hours(1) - base::Milliseconds(1),
          .expected = WindowResult::kNotStarted,
      },
      {
          .trigger_moment = base::Hours(1),
          .expected = WindowResult::kFallsWithin,
      },
      {
          .trigger_moment = base::Hours(2) - base::Milliseconds(1),
          .expected = WindowResult::kFallsWithin,
      },
      {
          .trigger_moment = base::Hours(2),
          .expected = WindowResult::kPassed,
      }};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(kDefaultReportWindows.FallsWithin(test_case.trigger_moment),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, Serialize) {
  const struct {
    EventReportWindows input;
    base::Value::Dict expected;
  } kTestCases[] = {
      {
          *EventReportWindows::CreateSingularWindow(base::Days(1)),
          base::test::ParseJsonDict(
              R"json({"event_report_window": 86400})json"),
      },
      {
          *EventReportWindows::CreateWindows(base::Seconds(0),
                                             {base::Days(1), base::Days(5)}),
          base::test::ParseJsonDict(R"json({"event_report_windows": {
            "start_time":0,
            "end_times":[86400,432000]
          }})json"),
      },
      {
          *EventReportWindows::CreateWindows(base::Hours(1),
                                             {base::Days(1), base::Days(5)}),
          base::test::ParseJsonDict(R"json({"event_report_windows": {
            "start_time":3600,
            "end_times":[86400,432000]
          }})json"),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value::Dict dict;
    test_case.input.Serialize(dict);
    EXPECT_EQ(dict, test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
