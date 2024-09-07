// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/time_utils.h"

#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {
namespace weekly_time_utils {

namespace {

enum {
  kMonday = 1,
  kTuesday = 2,
  kWednesday = 3,
  kThursday = 4,
  kFriday = 5,
  kSaturday = 6,
  kSunday = 7,
};

constexpr base::Time::Exploded kDaylightSavingsTime = {.year = 2018,
                                                       .month = 8,
                                                       .day_of_week = 3,
                                                       .day_of_month = 8,
                                                       .hour = 15};
constexpr base::Time::Exploded kNonDaylightSavingsTime{.year = 2018,
                                                       .month = 1,
                                                       .day_of_month = 28};

base::Time TimeFromString(const char* text) {
  base::Time time;
  const bool success = base::Time::FromString(text, &time);
  DCHECK(success);
  return time;
}

}  // namespace

class TimeUtilsTimezoneFunctionsTest : public testing::Test {
 protected:
  void SetUp() override {
    // Daylight savings are off by default
    SetDaylightSavings(false);
    timezone_.reset(icu::TimeZone::createDefault());
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone("America/New_York"));
  }

  void TearDown() override { icu::TimeZone::adoptDefault(timezone_.release()); }

  void SetDaylightSavings(bool is_daylight_savings) {
    if (is_daylight_savings) {
      // Friday July 13th
      base::Time test_time;
      ASSERT_TRUE(
          base::Time::FromUTCExploded(kDaylightSavingsTime, &test_time));
      test_clock_.SetNow(test_time);
    } else {
      // Sunday January 28
      base::Time test_time;
      ASSERT_TRUE(
          base::Time::FromUTCExploded(kNonDaylightSavingsTime, &test_time));
      test_clock_.SetNow(test_time);
    }
  }

  base::SimpleTestClock test_clock_;

 private:
  std::unique_ptr<icu::TimeZone> timezone_;
};

TEST_F(TimeUtilsTimezoneFunctionsTest, GetOffsetFromTimezoneToGmt) {
  // GMT + 7
  auto zone = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("Asia/Jakarta")));
  int result;
  GetOffsetFromTimezoneToGmt(*zone, &test_clock_, &result);
  // Negative since it's a conversion from |timezone| to GMT.
  EXPECT_EQ(result, base::Hours(-7).InMilliseconds());
  // Passing in the string should also work and yield the same result.
  int result2;
  GetOffsetFromTimezoneToGmt("Asia/Jakarta", &test_clock_, &result2);
  EXPECT_EQ(result2, result);
}

TEST_F(TimeUtilsTimezoneFunctionsTest, GetOffsetFromTimezoneToGmtDaylight) {
  SetDaylightSavings(true);
  // GMT -7.
  auto zone = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("America/Los_Angeles")));
  int result;
  GetOffsetFromTimezoneToGmt(*zone, &test_clock_, &result);
  EXPECT_EQ(result, base::Hours(7).InMilliseconds());
  // Passing in the string should also work and yield the same result.
  int result2;
  GetOffsetFromTimezoneToGmt("America/Los_Angeles", &test_clock_, &result2);
  EXPECT_EQ(result2, result);
}

TEST_F(TimeUtilsTimezoneFunctionsTest, GetOffsetFromTimezoneToGmtNoDaylight) {
  SetDaylightSavings(false);
  // GMT + 8.
  auto zone = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("America/Los_Angeles")));
  int result;
  GetOffsetFromTimezoneToGmt(*zone, &test_clock_, &result);
  EXPECT_EQ(result, base::Hours(8).InMilliseconds());
  // Passing in the string should also work and yield the same result.
  int result2;
  GetOffsetFromTimezoneToGmt("America/Los_Angeles", &test_clock_, &result2);
  EXPECT_EQ(result2, result);
}

TEST(TimeUtilsEmptyIntervalVector, NeverContainsTime) {
  EXPECT_FALSE(Contains(base::Time{}, {}));
  EXPECT_FALSE(Contains(base::Time::Now(), {}));
}

TEST(TimeUtilsEmptyIntervalVector, HasNoNextEvent) {
  EXPECT_EQ(GetNextEventTime(base::Time{}, {}), std::nullopt);
  EXPECT_EQ(GetNextEventTime(base::Time::Now(), {}), std::nullopt);
}

TEST(TimeUtilsNonEmptyIntervalVector, SometimesContainsTime) {
  const std::vector<WeeklyTimeInterval> intervals = {
      // First
      {WeeklyTime{kSunday, 0, 0},
       WeeklyTime{kSunday, base::Hours(2).InMilliseconds(), 0}},
      // Second (overlaps with first)
      {WeeklyTime{kSunday, base::Hours(1).InMilliseconds(), 0},
       WeeklyTime{kSunday, base::Hours(3).InMilliseconds(), 0}},
      // Third, no overlap
      {WeeklyTime{kMonday, 0, 0},
       WeeklyTime{kMonday, base::Hours(2).InMilliseconds() + 17, 0}},
  };

  // First interval, before begin.
  EXPECT_FALSE(
      Contains(TimeFromString("Sat, 17 Apr 2021 23:59:59 GMT"), intervals));
  // First interval, at begin.
  EXPECT_TRUE(
      Contains(TimeFromString("Sun, 18 Apr 2021 00:00:00 GMT"), intervals));
  // First interval, within.
  EXPECT_TRUE(
      Contains(TimeFromString("Sun, 18 Apr 2021 00:42:00 GMT"), intervals));
  // First interval, right before the end.
  EXPECT_TRUE(
      Contains(TimeFromString("Sun, 18 Apr 2021 01:59:59 GMT"), intervals));
  // First interval, at the end (but still in second interval).
  EXPECT_TRUE(
      Contains(TimeFromString("Sun, 18 Apr 2021 02:00:00 GMT"), intervals));

  // Second interval, right before the end.
  EXPECT_TRUE(
      Contains(TimeFromString("Sun, 18 Apr 2021 02:59:59 GMT"), intervals));
  // Second interval, at the end.
  EXPECT_FALSE(
      Contains(TimeFromString("Sun, 18 Apr 2021 03:00:00 GMT"), intervals));

  // Third interval, before begin.
  EXPECT_FALSE(
      Contains(TimeFromString("Sun, 18 Apr 2021 23:59:59 GMT"), intervals));
  // Third interval, begin.
  EXPECT_TRUE(
      Contains(TimeFromString("Mon, 19 Apr 2021 00:00:00 GMT"), intervals));
  // Third interval, within.
  EXPECT_TRUE(
      Contains(TimeFromString("Mon, 19 Apr 2021 00:42:00 GMT"), intervals));
  // Third interval, right before the end.
  EXPECT_TRUE(
      Contains(TimeFromString("Mon, 19 Apr 2021 02:00:00.016 GMT"), intervals));
  // Third interval, at the end.
  EXPECT_FALSE(
      Contains(TimeFromString("Mon, 19 Apr 2021 02:00:00.017 GMT"), intervals));

  // Far outside.
  EXPECT_FALSE(
      Contains(TimeFromString("Tue, 20 Apr 2021 22:05:00 GMT"), intervals));
}

TEST(TimeUtilsNonEmptyIntervalVector, AlwaysHasNextEvent) {
  const std::vector<WeeklyTimeInterval> intervals = {
      // First
      {WeeklyTime{kSunday, 0, 0},
       WeeklyTime{kSunday, base::Hours(2).InMilliseconds(), 0}},
      // Second (overlaps with first)
      {WeeklyTime{kSunday, base::Hours(1).InMilliseconds(), 0},
       WeeklyTime{kSunday, base::Hours(3).InMilliseconds(), 0}},
      // Third, no overlap
      {WeeklyTime{kMonday, 0, 0},
       WeeklyTime{kMonday, base::Hours(2).InMilliseconds() + 17, 0}},
  };

  // Just to make sure: Time::FromString actually parses microseconds
  ASSERT_EQ(TimeFromString("Tue, 20 Apr 2021 22:05:00.123456 GMT") -
                TimeFromString("Tue, 20 Apr 2021 22:05:00 GMT"),
            base::Microseconds(123456));

  // First interval, before begin.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sat, 17 Apr 2021 23:59:59 GMT"),
                             intervals),
            TimeFromString("Sun, 18 Apr 2021 00:00:00 GMT"));
  // First interval, at begin.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 00:00:00 GMT"),
                             intervals),
            TimeFromString("Sun, 18 Apr 2021 01:00:00 GMT"));
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 00:42:00 GMT"),
                             intervals),
            TimeFromString("Sun, 18 Apr 2021 01:00:00 GMT"));
  // First interval, right before the end.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 01:59:59 GMT"),
                             intervals),
            TimeFromString("Sun, 18 Apr 2021 02:00:00 GMT"));
  // First interval, at the end (but still in second interval).
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 02:00:00 GMT"),
                             intervals),
            TimeFromString("Sun, 18 Apr 2021 03:00:00 GMT"));

  // Second interval, right before the end.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 02:59:59 GMT"),
                             intervals),
            TimeFromString("Sun, 18 Apr 2021 03:00:00 GMT"));
  // Second interval, at the end.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 03:00:00 GMT"),
                             intervals),
            TimeFromString("Mon, 19 Apr 2021 00:00:00 GMT"));

  // Third interval, before begin.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Sun, 18 Apr 2021 23:59:59 GMT"),
                             intervals),
            TimeFromString("Mon, 19 Apr 2021 00:00:00 GMT"));
  // Third interval, begin.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Mon, 19 Apr 2021 00:00:00 GMT"),
                             intervals),
            TimeFromString("Mon, 19 Apr 2021 02:00:00.017 GMT"));
  // Third interval, within.
  EXPECT_EQ(GetNextEventTime(TimeFromString("Mon, 19 Apr 2021 00:42:00 GMT"),
                             intervals),
            TimeFromString("Mon, 19 Apr 2021 02:00:00.017 GMT"));
  // Third interval, right before the end.
  EXPECT_EQ(GetNextEventTime(
                TimeFromString("Mon, 19 Apr 2021 02:00:00.016 GMT"), intervals),
            TimeFromString("Mon, 19 Apr 2021 02:00:00.017 GMT"));
  // Third interval, at the end.
  EXPECT_EQ(GetNextEventTime(
                TimeFromString("Mon, 19 Apr 2021 02:00:00.017 GMT"), intervals),
            TimeFromString("Sun, 25 Apr 2021 00:00:00. GMT"));

  // Far outside, with non-zero microseconds.
  EXPECT_EQ(
      GetNextEventTime(TimeFromString("Tue, 20 Apr 2021 22:05:00.111111 GMT"),
                       intervals),
      TimeFromString("Sun, 25 Apr 2021 00:00:00 GMT"));

  // Far outside, with non-zero microseconds.
  EXPECT_EQ(
      GetNextEventTime(TimeFromString("Tue, 20 Apr 2021 22:05:00.888888 GMT"),
                       intervals),
      TimeFromString("Sun, 25 Apr 2021 00:00:00 GMT"));
}

}  // namespace weekly_time_utils
}  // namespace policy
