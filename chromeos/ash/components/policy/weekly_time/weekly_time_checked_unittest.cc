// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr const char* kTimeString = "20 Aug 2024 10:00";

constexpr int64_t kMillisecondsInHour = base::Hours(1).InMilliseconds();

constexpr base::TimeDelta kHour = base::Hours(1);
constexpr base::TimeDelta kDay = base::Days(1);

using Day = WeeklyTimeChecked::Day;
using weekly_time::BuildWeeklyTimeCheckedDict;
using weekly_time::DayToString;
using weekly_time::TimeFromString;

}  // namespace

TEST(WeeklyTimeCheckedTest, NextDay) {
  // clang-format off
  const struct TestData {
    Day day;
    Day expected_next_day;
  } kTestData[] = {
    {Day::kMonday, Day::kTuesday},
    {Day::kTuesday, Day::kWednesday},
    {Day::kWednesday, Day::kThursday},
    {Day::kThursday, Day::kFriday},
    {Day::kFriday, Day::kSaturday},
    {Day::kSaturday, Day::kSunday},
    {Day::kSunday, Day::kMonday},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    Day next_day = WeeklyTimeChecked::NextDay(t.day);
    SCOPED_TRACE(testing::Message() << "day: " << DayToString(t.day));
    EXPECT_EQ(next_day, t.expected_next_day);
  }
}

TEST(WeeklyTimeCheckedTest, Equality_True) {
  const int kMilliseconds = 111;
  auto w1 = WeeklyTimeChecked(Day::kMonday, kMilliseconds);
  auto w2 = WeeklyTimeChecked(Day::kMonday, kMilliseconds);
  EXPECT_EQ(w1, w2);
}

TEST(WeeklyTimeCheckedTest, Equality_FalseDay) {
  const int kMilliseconds = 111;
  auto w1 = WeeklyTimeChecked(Day::kMonday, kMilliseconds);
  auto w2 = WeeklyTimeChecked(Day::kTuesday, kMilliseconds);
  EXPECT_NE(w1, w2);
}

TEST(WeeklyTimeCheckedTest, Equality_FalseMillis) {
  auto w1 = WeeklyTimeChecked(Day::kMonday, 111);
  auto w2 = WeeklyTimeChecked(Day::kMonday, 222);
  EXPECT_NE(w1, w2);
}

TEST(WeeklyTimeCheckedTest, FromDict) {
  const int kMilliseconds = 123456;
  auto dict = BuildWeeklyTimeCheckedDict(Day::kWednesday, kMilliseconds);
  auto result = WeeklyTimeChecked::FromDict(dict);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->day_of_week(), Day::kWednesday);
  EXPECT_EQ(result->milliseconds_since_midnight(), kMilliseconds);
}

TEST(WeeklyTimeCheckedTest, FromDict_MissingDayOfWeek) {
  auto dict = base::JSONReader::ReadDict(R"({
    "milliseconds_since_midnight": 1234
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeCheckedTest, FromDict_InvalidDayOfWeek) {
  auto dict = base::JSONReader::ReadDict(R"({
    "day_of_week": "FOOBAR",
    "milliseconds_since_midnight": 1234
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeCheckedTest, FromDict_MissingMillisecondsSinceMidnight) {
  auto dict = base::JSONReader::ReadDict(R"({
    "day_of_week": "MONDAY"
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeCheckedTest, FromDict_InvalidMillisecondsSinceMidnight) {
  auto dict = base::JSONReader::ReadDict(R"({
    "day_of_week": "MONDAY",
    "milliseconds_since_midnight": -1
  })");
  ASSERT_TRUE(dict.has_value());
  auto result = WeeklyTimeChecked::FromDict(*dict);
  EXPECT_FALSE(result.has_value());
}

TEST(WeeklyTimeCheckedTest, FromExploded) {
  base::Time time = TimeFromString(kTimeString);
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  auto w = WeeklyTimeChecked::FromExploded(exploded);
  EXPECT_EQ(w.day_of_week(), Day::kTuesday);
  EXPECT_EQ(w.milliseconds_since_midnight(), 10 * kMillisecondsInHour);
}

TEST(WeeklyTimeCheckedTest, FromTime) {
  base::Time time = TimeFromString(kTimeString);
  auto w = WeeklyTimeChecked::FromTimeAsLocalTime(time);
  EXPECT_EQ(w.day_of_week(), Day::kTuesday);
  EXPECT_EQ(w.milliseconds_since_midnight(), 10 * kMillisecondsInHour);
}

TEST(WeeklyTimeCheckedTest, FromTimeDelta) {
  // clang-format off
  struct TestData {
    base::TimeDelta time_delta;
    Day day;
    int millis;
  } test_data[] = {
    {base::TimeDelta(), Day::kMonday, 0},
    {base::Milliseconds(100), Day::kMonday, 100},
    {base::Days(7), Day::kMonday, 0},
    {base::Days(8), Day::kTuesday, 0},
    {base::Days(16), Day::kWednesday, 0},
    {base::Days(16) + base::Milliseconds(100), Day::kWednesday, 100},
    // Negative values.
    {-base::Days(1), Day::kSunday, 0},
    {-base::Days(2), Day::kSaturday, 0},
    {-base::Days(3), Day::kFriday, 0},
    {-base::Days(7), Day::kMonday, 0},
    {-base::Days(8), Day::kSunday, 0},
    {-base::Days(1) - base::Hours(1), Day::kSaturday, base::Hours(23).InMilliseconds()},
    {-base::Days(1) - base::Hours(23), Day::kSaturday, base::Hours(1).InMilliseconds()},
    {-base::Days(6) - base::Hours(1), Day::kMonday, base::Hours(23).InMilliseconds()},
    {-base::Days(6) - base::Hours(23), Day::kMonday, base::Hours(1).InMilliseconds()},
    {-base::Days(6) - base::Hours(24), Day::kMonday, 0},
  };
  // clang-format on

  int i = 0;
  for (const auto& t : test_data) {
    auto actual = WeeklyTimeChecked::FromTimeDelta(t.time_delta);
    auto expected = WeeklyTimeChecked(t.day, t.millis);
    EXPECT_EQ(actual, expected) << "Failed test case #" << i;
    i++;
  }
}

TEST(WeeklyTimeCheckedTest, ToTimeDelta) {
  auto w = WeeklyTimeChecked(Day::kTuesday, 3 * kMillisecondsInHour);
  auto td = w.ToTimeDelta();
  EXPECT_EQ(td, (2 - 1) * kDay + 3 * kHour);
}

}  // namespace policy
