// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/checked_util.h"

#include <optional>
#include <vector>

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

}  // namespace policy::weekly_time
