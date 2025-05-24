// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/checked_util.h"

#include <optional>
#include <vector>

#include "base/scoped_environment_variable_override.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/test_support.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::weekly_time {

namespace {

constexpr const char* kIntervalsJson = R"([
  {
    "start": {
        "day_of_week": "WEDNESDAY",
        "milliseconds_since_midnight": 43200000
    },
    "end": {
        "day_of_week": "WEDNESDAY",
        "milliseconds_since_midnight": 75600000
    }
  },
  {
    "start": {
        "day_of_week": "FRIDAY",
        "milliseconds_since_midnight": 64800000
    },
    "end": {
        "day_of_week": "MONDAY",
        "milliseconds_since_midnight": 21600000
    }
  }
])";

constexpr const char kTZ[] = "TZ";

using Day = WeeklyTimeChecked::Day;

}  // namespace

TEST(CheckedUtil, ExtractIntervalsFromList) {
  base::Value::List list = BuildList(kIntervalsJson);
  auto result = ExtractIntervalsFromList(list);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
}

TEST(CheckedUtil, ExtractIntervalsFromList_NotADict) {
  base::Value::List list = BuildList(R"([123])");

  auto result = ExtractIntervalsFromList(list);
  EXPECT_FALSE(result.has_value());
}

TEST(CheckedUtil, ExtractIntervalsFromList_InvalidInterval) {
  base::Value::List list = BuildList(R"([
    {
      "foobarbaz": {
          "day_of_week": "WEDNESDAY",
          "milliseconds_since_midnight": 43200000
      },
      "end": {
          "day_of_week": "WEDNESDAY",
          "milliseconds_since_midnight": 75600000
      }
    }
  ])");

  auto result = ExtractIntervalsFromList(list);
  EXPECT_FALSE(result.has_value());
}

TEST(CheckedUtil, IntervalsContainTime_True) {
  std::vector<WeeklyTimeIntervalChecked> intervals =
      BuildIntervals(kIntervalsJson);

  // Saturday is fully included in the second interval.
  WeeklyTimeChecked time(Day::kSaturday, 1234);
  EXPECT_TRUE(IntervalsContainTime(intervals, time));
}

TEST(CheckedUtil, IntervalsContainTime_False) {
  std::vector<WeeklyTimeIntervalChecked> intervals =
      BuildIntervals(kIntervalsJson);

  // Tuesday isn't even partially included in any of the intervals.
  WeeklyTimeChecked time(Day::kTuesday, 1234);
  EXPECT_FALSE(IntervalsContainTime(intervals, time));
}

TEST(CheckedUtil, IntervalsContainTime_Empty) {
  std::vector<WeeklyTimeIntervalChecked> intervals;
  WeeklyTimeChecked time(Day::kMonday, 1234);
  EXPECT_FALSE(IntervalsContainTime(intervals, time));
}

TEST(CheckedUtil, GetNextEvent) {
  const struct TestData {
    Day day;
    int millis;
    WeeklyTimeChecked next_event;
  } kTestData[] = {
      // One day before the start of the first interval.
      {Day::kTuesday, 43200000, WeeklyTimeChecked(Day::kWednesday, 43200000)},
      // Start of the first interval.
      {Day::kWednesday, 43200000, WeeklyTimeChecked(Day::kWednesday, 75600000)},
      // Inside first interval.
      {Day::kWednesday, 57600000, WeeklyTimeChecked(Day::kWednesday, 75600000)},
      // End of first interval.
      {Day::kWednesday, 75600000, WeeklyTimeChecked(Day::kFriday, 64800000)},
      // One hour before the second interval.
      {Day::kFriday, 61200000, WeeklyTimeChecked(Day::kFriday, 64800000)},
  };

  std::vector<WeeklyTimeIntervalChecked> intervals =
      BuildIntervals(kIntervalsJson);

  for (const auto& t : kTestData) {
    WeeklyTimeChecked time(t.day, t.millis);
    std::optional<WeeklyTimeChecked> next_event = GetNextEvent(intervals, time);
    SCOPED_TRACE(testing::Message()
                 << "day: " << DayToString(t.day) << ", millis: " << t.millis);
    ASSERT_TRUE(next_event.has_value());
    EXPECT_EQ(next_event.value(), t.next_event);
  }
}

TEST(CheckedUtil, GetNextEvent_Empty) {
  std::vector<WeeklyTimeIntervalChecked> intervals;
  WeeklyTimeChecked time(Day::kMonday, 1234);
  std::optional<WeeklyTimeChecked> next_event = GetNextEvent(intervals, time);
  EXPECT_FALSE(next_event.has_value());
}

TEST(CheckedUtil, GetDurationToNextEvent) {
  std::vector<WeeklyTimeIntervalChecked> intervals =
      BuildIntervals(kIntervalsJson);

  struct TestData {
    Day day;
    int millis;
    base::TimeDelta duration;
  } test_data[] = {
      // One day before the start of the first interval.
      {Day::kTuesday, 43200000, base::Days(1)},
      // Start of the first interval, returns the duration until the end of the
      // first interval.
      {Day::kWednesday, 43200000, base::Hours(9)},
      // Inside first interval.
      {Day::kWednesday, 57600000, base::Hours(5)},
      // End of first interval.
      {Day::kWednesday, 75600000, base::Days(2) - base::Hours(3)},
      // One hour before the second interval.
      {Day::kFriday, 61200000, base::Hours(1)},
  };

  int i = 0;
  for (const auto& t : test_data) {
    WeeklyTimeChecked time(t.day, t.millis);
    std::optional<base::TimeDelta> duration =
        GetDurationToNextEvent(intervals, time);
    ASSERT_TRUE(duration.has_value()) << "Failed test case #" << i;
    EXPECT_EQ(duration.value(), t.duration) << "Failed test case #" << i;
    i++;
  }
}

TEST(CheckedUtil, GetDurationToNextEvent_Empty) {
  std::vector<WeeklyTimeIntervalChecked> intervals;
  WeeklyTimeChecked time(Day::kMonday, 1234);
  auto duration = GetDurationToNextEvent(intervals, time);
  EXPECT_FALSE(duration.has_value());
}

TEST(CheckedUtil, AddOffsetInLocalTime) {
  // clang-format off
  constexpr const struct TestData {
    const char* time_str;
    base::TimeDelta offset;
    const char* result_str;
  } kTestData[] = {
    // All times are in local time (time zone "Europe/Berlin").
    // DST starts on Sun, 31 Mar 2024 when the clock moves from 2:00 to 3:00.
    // DST ends   on Sun, 27 Oct 2024 when the clock moves from 3:00 to 2:00.

    // Positive offset.
    {"Fri 22 Mar 2024 19:00", base::Seconds(1), "Fri 22 Mar 2024 19:00:01"},
    {"Fri 22 Mar 2024 19:00", base::Minutes(1), "Fri 22 Mar 2024 19:01"},
    {"Fri 22 Mar 2024 19:00", base::Hours(1),   "Fri 22 Mar 2024 20:00"},
    {"Fri 22 Mar 2024 19:00", base::Days(1),    "Sat 23 Mar 2024 19:00"},
    // One year, crosses a double DST-boundary.
    {"Fri 22 Mar 2024 19:00", base::Days(365),  "Sat 22 Mar 2025 19:00"},

    // Negative offset.
    {"Fri 22 Mar 2024 19:00", -base::Seconds(1), "Fri 22 Mar 2024 18:59:59"},
    {"Fri 22 Mar 2024 19:00", -base::Minutes(1), "Fri 22 Mar 2024 18:59"},
    {"Fri 22 Mar 2024 19:00", -base::Hours(1),   "Fri 22 Mar 2024 18:00"},
    {"Fri 22 Mar 2024 19:00", -base::Days(1),    "Thu 21 Mar 2024 19:00"},
    // One (leap) year, crosses a double DST-boundary.
    {"Fri 22 Mar 2024 19:00", -base::Days(366),  "Wed 22 Mar 2023 19:00"},

    // Positive offset crossing DST-boundary Winter -> Summer (2:00 -> 3:00).
    {"Sun 31 Mar 2024 1:00", base::Hours(2), "Sun 31 Mar 2024 3:00"},
    {"Sun 31 Mar 2024 0:00", base::Hours(4), "Sun 31 Mar 2024 4:00"},
    // Normal result time doesn't exist in local time (clock moves 2 -> 3), so
    // the calculation is done in UTC time.
    {"Sun 31 Mar 2024 1:59", base::Minutes(1), "Sun 31 Mar 2024 3:00"},
    {"Sun 31 Mar 2024 1:20", base::Hours(1),   "Sun 31 Mar 2024 3:20"},

    // Negative offset crossing DST-boundary Winter -> Summer (2:00 -> 3:00).
    {"Sun 31 Mar 2024 3:00", -base::Hours(2), "Sun 31 Mar 2024 1:00"},

    // Positive offset crossing DST-boundary Summer -> Winter (3:00 -> 2:00).
    {"Sat 26 Oct 2024 22:00", base::Days(1), "Sun 27 Oct 2024 22:00"},

    // Negative offset crossing DST-boundary Summer -> Winter (3:00 -> 2:00).
    {"Sun 27 Oct 2024 3:30", -base::Hours(2), "Sun 27 Oct 2024 1:30"},
  };
  // clang-format on

  // Override the local time zone to fix the DST transitions.
  base::ScopedEnvironmentVariableOverride scoped_timezone(kTZ, "Europe/Berlin");

  for (const auto& t : kTestData) {
    SCOPED_TRACE(testing::Message()
                 << "time: " << t.time_str << ", offset: " << t.offset
                 << ", result: " << t.result_str);

    const base::Time time = TimeFromString(t.time_str);
    const base::Time result = AddOffsetInLocalTime(time, t.offset);
    const base::Time expected_result = TimeFromString(t.result_str);

    EXPECT_EQ(expected_result, result);
  }
}

}  // namespace policy::weekly_time
