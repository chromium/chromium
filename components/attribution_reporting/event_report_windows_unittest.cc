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

constexpr base::TimeDelta kWindowDeadlineOffset = base::Hours(1);

TEST(EventReportWindowsTest, Create) {
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
          .expected = EventReportWindows::Create(
              base::Seconds(0), {base::Seconds(1), base::Seconds(2)}),
      },
  };
  for (const auto& test_case : kTestCases) {
    auto windows =
        EventReportWindows::Create(test_case.start_time, test_case.end_times);
    EXPECT_EQ(windows, test_case.expected);
  }
}

TEST(EventReportWindowsTest, CreateAndTruncate) {
  base::TimeDelta start_time = base::Seconds(5);
  std::vector<base::TimeDelta> end_times = {base::Seconds(10),
                                            base::Seconds(30)};

  const struct {
    base::TimeDelta expiry;
    absl::optional<EventReportWindows> expected;
  } kTestCases[] = {
      {.expiry = base::Seconds(5), .expected = absl::nullopt},
      {.expiry = base::Seconds(6),
       .expected = EventReportWindows::Create(start_time, {base::Seconds(6)})},
      {.expiry = base::Seconds(10),
       .expected = EventReportWindows::Create(start_time, {base::Seconds(10)})},
      {.expiry = base::Seconds(11),
       .expected = EventReportWindows::Create(
           start_time, {base::Seconds(10), base::Seconds(11)})},
      {.expiry = base::Seconds(31),
       .expected = EventReportWindows::Create(
           start_time,
           {base::Seconds(10), base::Seconds(30), base::Seconds(31)})},
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(EventReportWindows::CreateAndTruncate(start_time, end_times,
                                                    test_case.expiry),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, Parse) {
  const struct {
    const char* desc;
    base::Value json;
    base::expected<EventReportWindows, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "wrong_type",
          base::test::ParseJson(R"json(0)json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsWrongType),
      },
      {
          "empty_dict",
          base::test::ParseJson(R"json({})json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesMissing),
      },
      {
          "start_time_wrong_type",
          base::test::ParseJson(R"json({
            "start_time":"0",
            "end_times":[96000,172800]
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsStartTimeWrongType),
      },
      {
          "start_time_negative",
          base::test::ParseJson(R"json({
            "start_time":-3600,
            "end_times":[96000,172800]
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsStartTimeInvalid),
      },
      {
          "end_times_missing",
          base::test::ParseJson(R"json({
            "start_time":0
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesMissing),
      },
      {
          "end_times_wrong_type",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":96000
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesWrongType),
      },
      {
          "end_times_list_empty",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":[]
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesListEmpty),
      },
      {
          "end_times_list_too_long",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":[3600,7200,10800,14400,18000,21600]
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimesListTooLong),
      },
      {
          "end_times_value_wrong_type",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":["3600"]
          })json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeValueWrongType),
      },
      {
          "end_times_value_negative",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":[-3600]
          })json"),
          base::unexpected(
              SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid),
      },
      {
          "start_time_equal_end",
          base::test::ParseJson(R"json({
            "start_time":3600,
            "end_times":[3600]
          })json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "start_duration_equal_end",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":[3600,3600]
          })json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "start_duration_greater_than_end",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":[5400,3600]
          })json"),
          base::unexpected(SourceRegistrationError::
                               kEventReportWindowsEndTimeDurationLTEStart),
      },
      {
          "valid",
          base::test::ParseJson(R"json({
            "start_time":0,
            "end_times":[3600,10800,21600]
          })json"),
          *EventReportWindows::Create(
              base::Seconds(0), {base::Seconds(3600), base::Seconds(10800),
                                 base::Seconds(21600)}),
      },
      {
          "valid_start_time_missing",
          base::test::ParseJson(R"json({
            "end_times":[3600,10800,21600]
          })json"),
          *EventReportWindows::Create(
              base::Seconds(0), {base::Seconds(3600), base::Seconds(10800),
                                 base::Seconds(21600)}),
      },
      {
          "valid_start_time_set",
          base::test::ParseJson(R"json({
            "start_time":7200,
            "end_times":[16000,32000,48000]
          })json"),
          *EventReportWindows::Create(
              base::Seconds(7200), {base::Seconds(16000), base::Seconds(32000),
                                    base::Seconds(48000)}),
      },
      {
          "valid_end_time_less_than_default",
          base::test::ParseJson(R"json({
            "end_times":[1800]
          })json"),
          *EventReportWindows::Create(base::Seconds(0), {base::Seconds(3600)}),
      },
  };

  for (const auto& test_case : kTestCases) {
    auto event_report_windows = EventReportWindows::FromJSON(test_case.json);
    EXPECT_EQ(test_case.expected, event_report_windows) << test_case.desc;
  }
}

TEST(EventReportWindowsTest, ComputeReportTime) {
  const EventReportWindows kDefaultReportWindows = *EventReportWindows::Create(
      base::Hours(0), {base::Hours(2), base::Days(1), base::Days(7)});
  base::Time source_time = base::Time();
  const struct {
    base::Time trigger_time;
    base::Time expected;
  } kTestCases[] = {
      {
          .trigger_time = source_time,
          .expected = source_time + base::Hours(2) + kWindowDeadlineOffset,
      },
      {
          .trigger_time = source_time + base::Hours(2),
          .expected = source_time + base::Hours(2) + kWindowDeadlineOffset,
      },
      {
          .trigger_time = source_time + base::Hours(2) + base::Milliseconds(1),
          .expected = source_time + base::Days(1) + kWindowDeadlineOffset,
      },
      {
          .trigger_time = source_time + base::Days(1),
          .expected = source_time + base::Days(1) + kWindowDeadlineOffset,
      },
      {
          .trigger_time = source_time + base::Days(1) + base::Milliseconds(1),
          .expected = source_time + base::Days(7) + kWindowDeadlineOffset,
      },
      {
          .trigger_time = source_time + base::Days(7),
          .expected = source_time + base::Days(7) + kWindowDeadlineOffset,
      }};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(kDefaultReportWindows.ComputeReportTime(source_time,
                                                      test_case.trigger_time),
              test_case.expected);
  }
}

TEST(EventReportWindowsTest, ReportTimeAtWindow) {
  const EventReportWindows kDefaultReportWindows = *EventReportWindows::Create(
      base::Hours(0), {base::Hours(1), base::Days(3), base::Days(7)});
  base::Time source_time = base::Time();
  const struct {
    int index;
    base::Time expected;
  } kTestCases[] = {
      {
          .index = 0,
          .expected = source_time + base::Hours(1) + kWindowDeadlineOffset,
      },
      {
          .index = 1,
          .expected = source_time + base::Days(3) + kWindowDeadlineOffset,
      },
      {
          .index = 2,
          .expected = source_time + base::Days(7) + kWindowDeadlineOffset,
      }};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        kDefaultReportWindows.ReportTimeAtWindow(source_time, test_case.index),
        test_case.expected);
  }
}

TEST(EventReportWindowsTest, FallsWithin) {
  const EventReportWindows kDefaultReportWindows =
      *EventReportWindows::Create(base::Hours(1), {base::Hours(2)});
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

TEST(EventReportWindowsTest, ToJson) {
  const struct {
    EventReportWindows input;
    const char* expected_json;
  } kTestCases[] = {
      {
          *EventReportWindows::Create(base::Seconds(0),
                                      {base::Days(1), base::Days(5)}),
          R"json({
            "start_time":0,
            "end_times":[86400,432000]
          })json",
      },
      {
          *EventReportWindows::Create(base::Hours(1),
                                      {base::Days(1), base::Days(5)}),
          R"json({
            "start_time":3600,
            "end_times":[86400,432000]
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
