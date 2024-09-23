// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"

#include <array>
#include <optional>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr int64_t kMillisecondsInMinute = base::Minutes(1).InMilliseconds();
constexpr int64_t kMillisecondsInHour = base::Hours(1).InMilliseconds();
constexpr int64_t kMillisecondsInDay = base::Days(1).InMilliseconds();
constexpr int kMinutesInHour = base::Hours(1).InMinutes();

constexpr base::TimeDelta kMillisecond = base::Milliseconds(1);
constexpr base::TimeDelta kHour = base::Hours(1);
constexpr base::TimeDelta kDay = base::Days(1);
constexpr base::TimeDelta kWeek = base::Days(7);

using Day = WeeklyTimeChecked::Day;
using weekly_time::BuildWeeklyTimeIntervalCheckedDict;

}  // namespace

TEST(WeeklyTimeIntervalCheckedTest, Equality_True) {
  auto interval =
      WeeklyTimeIntervalChecked(WeeklyTimeChecked(Day::kFriday, 111),
                                WeeklyTimeChecked(Day::kSaturday, 222));
  EXPECT_EQ(interval, interval);
}

TEST(WeeklyTimeIntervalCheckedTest, Equality_FalseStart) {
  auto w_end = WeeklyTimeChecked(Day::kSaturday, 222);
  auto interval1 =
      WeeklyTimeIntervalChecked(WeeklyTimeChecked(Day::kTuesday, 111), w_end);
  auto interval2 =
      WeeklyTimeIntervalChecked(WeeklyTimeChecked(Day::kFriday, 111), w_end);
  EXPECT_NE(interval1, interval2);
}

TEST(WeeklyTimeIntervalCheckedTest, Equality_FalseEnd) {
  auto w_start = WeeklyTimeChecked(Day::kSaturday, 222);
  auto interval1 = WeeklyTimeIntervalChecked(
      w_start, WeeklyTimeChecked(Day::kWednesday, 111));
  auto interval2 =
      WeeklyTimeIntervalChecked(w_start, WeeklyTimeChecked(Day::kSunday, 111));
  EXPECT_NE(interval1, interval2);
}

TEST(WeeklyTimeIntervalCheckedTest, FromDict) {
  const int kMillis1 = 111, kMillis2 = 222;
  auto dict = BuildWeeklyTimeIntervalCheckedDict(Day::kFriday, kMillis1,
                                                 Day::kMonday, kMillis2);
  auto result = WeeklyTimeIntervalChecked::FromDict(dict);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->start().day_of_week(), Day::kFriday);
  EXPECT_EQ(result->start().milliseconds_since_midnight(), kMillis1);
  EXPECT_EQ(result->end().day_of_week(), Day::kMonday);
  EXPECT_EQ(result->end().milliseconds_since_midnight(), kMillis2);
}

TEST(WeeklyTimeIntervalCheckedTest, FromDict_MissingStart) {
  auto dict = base::JSONReader::ReadDict(R"({
    "end": {
      "day_of_week": "TUESDAY",
      "milliseconds_since_midnight": 321
    }
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeIntervalChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeIntervalCheckedTest, FromDict_MissingEnd) {
  auto dict = base::JSONReader::ReadDict(R"({
    "start": {
      "day_of_week": "MONDAY",
      "milliseconds_since_midnight": 123
    }
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeIntervalChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeIntervalCheckedTest, FromDict_InvalidStart) {
  auto dict = base::JSONReader::ReadDict(R"({
    "start": {
      "day_of_weeeek": "MONDAY",
      "milliseconds_since_midnight": 123
    },
    "end": {
      "day_of_week": "TUESDAY",
      "milliseconds_since_midnight": 321
    }
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeIntervalChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeIntervalCheckedTest, Duration_Week) {
  auto w = WeeklyTimeChecked(Day::kTuesday, 0);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w, w).Duration(), kWeek);
}

TEST(WeeklyTimeIntervalCheckedTest, Duration_ShortestAndLongest) {
  auto w1 = WeeklyTimeChecked(Day::kMonday, 0);
  auto w2 = WeeklyTimeChecked(Day::kSunday, kMillisecondsInDay - 1);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w1, w2).Duration(), kWeek - kMillisecond);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w2, w1).Duration(), kMillisecond);
}

TEST(WeeklyTimeIntervalCheckedTest, Duration_OneDay) {
  auto w1 = WeeklyTimeChecked(Day::kThursday, 0);
  auto w2 = WeeklyTimeChecked(Day::kFriday, 0);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w1, w2).Duration(), kDay);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w2, w1).Duration(), 6 * kDay);
}

TEST(WeeklyTimeIntervalCheckedTest, Duration_OneHour) {
  auto w1 = WeeklyTimeChecked(Day::kSaturday, 10 * kMillisecondsInHour);
  auto w2 = WeeklyTimeChecked(Day::kSaturday, 11 * kMillisecondsInHour);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w1, w2).Duration(), kHour);
  EXPECT_EQ(WeeklyTimeIntervalChecked(w2, w1).Duration(), kWeek - kHour);
}

TEST(WeeklyTimeIntervalCheckedTest, Contains) {
  // clang-format off
  struct TestData {
    Day start_day; int start_minutes;
    Day end_day; int end_minutes;
    Day cur_day; int cur_minutes;
    bool expected_contains;
  } test_data[] = {
      // Longest interval.
      {
        Day::kMonday, 0,
        Day::kSunday, 24 * kMinutesInHour - 1,
        Day::kWednesday, 10 * kMinutesInHour,
        true
      },
      // Shortest intervals.
      {
        Day::kMonday, 0,
        Day::kMonday, 1,
        Day::kTuesday, 9 * kMinutesInHour,
        false
      }, {
        Day::kMonday, 0,
        Day::kMonday, 1,
        Day::kMonday, 1,
        false
      }, {
        Day::kMonday, 0,
        Day::kMonday, 1,
        Day::kMonday, 0,
        true
      }, {
        Day::kSunday, 24 * kMinutesInHour - 1,
        Day::kMonday, 0,
        Day::kWednesday, 10 * kMinutesInHour,
        false
      },
      // Start of interval.
      {
        Day::kTuesday, 10 * kMinutesInHour + 30,
        Day::kFriday, 14 * kMinutesInHour + 45,
        Day::kTuesday, 10 * kMinutesInHour + 30,
        true
      },
      // End of interval.
      {
        Day::kTuesday, 10 * kMinutesInHour + 30,
        Day::kFriday, 14 * kMinutesInHour + 45,
        Day::kFriday, 14 * kMinutesInHour + 45,
        false
      },
      // Wrap around intervals.
      {
        Day::kFriday, 17 * kMinutesInHour + 60,
        Day::kMonday, 9 * kMinutesInHour,
        Day::kSunday, 14 * kMinutesInHour,
        true
      }, {
        Day::kFriday, 17 * kMinutesInHour + 60,
        Day::kMonday, 9 * kMinutesInHour,
        Day::kWednesday, 14 * kMinutesInHour,
        false
      },
      // Random interval.
      {
        Day::kMonday, 9 * kMinutesInHour,
        Day::kFriday, 17 * kMinutesInHour,
        Day::kSunday, 14 * kMinutesInHour,
        false
      },
  };
  // clang-format on

  int i = 0;
  for (const auto& t : test_data) {
    auto interval = WeeklyTimeIntervalChecked(
        WeeklyTimeChecked(t.start_day, t.start_minutes * kMillisecondsInMinute),
        WeeklyTimeChecked(t.end_day, t.end_minutes * kMillisecondsInMinute));
    auto current =
        WeeklyTimeChecked(t.cur_day, t.cur_minutes * kMillisecondsInMinute);
    EXPECT_EQ(interval.Contains(current), t.expected_contains)
        << "Failed test case #" << i;
    i++;
  }
}

TEST(WeeklyTimeIntervalCheckedTest, IntervalsOverlap_False) {
  auto a = WeeklyTimeIntervalChecked(
      WeeklyTimeChecked(Day::kMonday, base::Hours(18).InMilliseconds()),
      WeeklyTimeChecked(Day::kTuesday, base::Hours(8).InMilliseconds()));

  auto b = WeeklyTimeIntervalChecked(
      WeeklyTimeChecked(Day::kFriday, base::Hours(8).InMilliseconds()),
      WeeklyTimeChecked(Day::kSaturday, base::Hours(8).InMilliseconds()));

  auto c = WeeklyTimeIntervalChecked(
      WeeklyTimeChecked(Day::kTuesday, base::Hours(8).InMilliseconds()),
      WeeklyTimeChecked(Day::kWednesday, base::Hours(14).InMilliseconds()));

  /*
  Mon         Tue         Wed         Thu         Fri         Sat         Sun
          |---------|                               |--------------|
               a                                            b
                    |------------|
                          c
  */
  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(a, b));
  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(b, a));

  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(a, c));
  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(c, a));

  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(b, c));
  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(c, b));
}

TEST(WeeklyTimeIntervalCheckedTest, IntervalsOverlap_True) {
  auto a = WeeklyTimeIntervalChecked(
      WeeklyTimeChecked(Day::kMonday, base::Hours(18).InMilliseconds()),
      WeeklyTimeChecked(Day::kThursday, base::Hours(18).InMilliseconds()));

  auto b = WeeklyTimeIntervalChecked(
      WeeklyTimeChecked(Day::kThursday, base::Hours(6).InMilliseconds()),
      WeeklyTimeChecked(Day::kFriday, base::Hours(13).InMilliseconds()));

  auto c = WeeklyTimeIntervalChecked(
      WeeklyTimeChecked(Day::kTuesday, base::Hours(8).InMilliseconds()),
      WeeklyTimeChecked(Day::kWednesday, base::Hours(14).InMilliseconds()));

  /*
  Mon         Tue         Wed         Thu         Fri         Sat         Sun
          |------------------------------------|
               a
                                         |----------------|
                                                  b
                    |------------|
                          c
  */
  EXPECT_TRUE(WeeklyTimeIntervalChecked::IntervalsOverlap(a, a));

  EXPECT_TRUE(WeeklyTimeIntervalChecked::IntervalsOverlap(a, b));
  EXPECT_TRUE(WeeklyTimeIntervalChecked::IntervalsOverlap(b, a));

  EXPECT_TRUE(WeeklyTimeIntervalChecked::IntervalsOverlap(a, c));
  EXPECT_TRUE(WeeklyTimeIntervalChecked::IntervalsOverlap(c, a));

  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(b, c));
  EXPECT_FALSE(WeeklyTimeIntervalChecked::IntervalsOverlap(c, b));
}

}  // namespace policy
