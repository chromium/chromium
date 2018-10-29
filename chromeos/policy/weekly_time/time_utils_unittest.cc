// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/policy/weekly_time/time_utils.h"

#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "chromeos/policy/weekly_time/weekly_time_interval.h"
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

constexpr int kMinutesInHour = 60;
constexpr int kMillisecondsInHour = 3600000;
constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::Time::Exploded kDaylightSavingsTime{2018, 8, 3, 8, 15, 0, 0, 0};
constexpr base::Time::Exploded kNonDaylightSavingsTime{2018, 1, 0, 28,
                                                       0,    0, 0, 0};

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

TEST_F(TimeUtilsTimezoneFunctionsTest, ToLocalizedStringDaylightSavings) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  SetDaylightSavings(true);

  // 15:50 UTC, 8:50 PT, 11:50 PT
  WeeklyTime test_weekly_time =
      WeeklyTime(5, (15 * kMinutesInHour + 50) * kMinute.InMilliseconds(), 0);

  base::i18n::SetICUDefaultLocale("en_US");
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone("America/Los_Angeles"));
  EXPECT_EQ(base::UTF8ToUTF16("Friday 8:50 AM"),
            WeeklyTimeToLocalizedString(test_weekly_time, &test_clock_));

  base::i18n::SetICUDefaultLocale("de_DE");
  EXPECT_EQ(base::UTF8ToUTF16("Freitag, 08:50"),
            WeeklyTimeToLocalizedString(test_weekly_time, &test_clock_));

  base::i18n::SetICUDefaultLocale("en_GB");
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone("America/New_York"));
  EXPECT_EQ(base::UTF8ToUTF16("Friday 11:50"),
            WeeklyTimeToLocalizedString(test_weekly_time, &test_clock_));
}

TEST_F(TimeUtilsTimezoneFunctionsTest, ToLocalizedStringNoDaylightSavings) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  SetDaylightSavings(false);

  // 15:50 UTC, 7:50 PST
  WeeklyTime test_weekly_time =
      WeeklyTime(5, (15 * kMinutesInHour + 50) * kMinute.InMilliseconds(), 0);

  base::i18n::SetICUDefaultLocale("en_US");
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone("America/Los_Angeles"));
  EXPECT_EQ(base::UTF8ToUTF16("Friday 7:50 AM"),
            WeeklyTimeToLocalizedString(test_weekly_time, &test_clock_));
}

TEST_F(TimeUtilsTimezoneFunctionsTest, GetOffsetFromTimezoneToGmt) {
  // GMT + 7
  auto zone = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("Asia/Jakarta")));
  int result;
  GetOffsetFromTimezoneToGmt(*zone, &test_clock_, &result);
  // Negative since it's a conversion from |timezone| to GMT.
  EXPECT_EQ(result, -7 * kHour.InMilliseconds());
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
  EXPECT_EQ(result, 7 * kHour.InMilliseconds());
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
  EXPECT_EQ(result, 8 * kHour.InMilliseconds());
  // Passing in the string should also work and yield the same result.
  int result2;
  GetOffsetFromTimezoneToGmt("America/Los_Angeles", &test_clock_, &result2);
  EXPECT_EQ(result2, result);
}

class GetIntervalForCurrentTimeTest
    : public testing::TestWithParam<
          std::tuple<std::vector<WeeklyTimeInterval>,
                     base::Optional<WeeklyTimeInterval>>> {
 protected:
  void SetUp() override {
    // Wednesday August 8th at 15:00 GMT
    base::Time test_time;
    ASSERT_TRUE(base::Time::FromUTCExploded(kDaylightSavingsTime, &test_time));
    test_clock_.SetNow(test_time);
  }

  std::vector<WeeklyTimeInterval> intervals() {
    return std::get<0>(GetParam());
  }
  base::Optional<WeeklyTimeInterval> expected_result() {
    return std::get<1>(GetParam());
  }

  base::SimpleTestClock test_clock_;
};

TEST_P(GetIntervalForCurrentTimeTest, Test) {
  base::Optional<WeeklyTimeInterval> result =
      GetIntervalForCurrentTime(intervals(), &test_clock_);
  EXPECT_EQ(result, expected_result());
}

INSTANTIATE_TEST_CASE_P(
    SameTimezoneNone,
    GetIntervalForCurrentTimeTest,
    testing::Values(std::make_tuple(
        std::vector<WeeklyTimeInterval>{
            WeeklyTimeInterval(
                WeeklyTime(kTuesday, 10 * kMillisecondsInHour, 0),
                WeeklyTime(kWednesday, 8 * kMillisecondsInHour, 0)),
            WeeklyTimeInterval(
                WeeklyTime(kSunday, 5 * kMillisecondsInHour, 0),
                WeeklyTime(kSunday, 16 * kMillisecondsInHour, 0))},
        base::nullopt)));

INSTANTIATE_TEST_CASE_P(
    SameTimezoneResult,
    GetIntervalForCurrentTimeTest,
    testing::Values(
        std::make_tuple(
            std::vector<WeeklyTimeInterval>{
                WeeklyTimeInterval(
                    WeeklyTime(kTuesday, 10 * kMillisecondsInHour, 0),
                    WeeklyTime(kThursday, 8 * kMillisecondsInHour, 0)),
                WeeklyTimeInterval(
                    WeeklyTime(kSunday, 5 * kMillisecondsInHour, 0),
                    WeeklyTime(kSunday, 16 * kMillisecondsInHour, 0))},
            WeeklyTimeInterval(
                WeeklyTime(kTuesday, 10 * kMillisecondsInHour, 0),
                WeeklyTime(kThursday, 8 * kMillisecondsInHour, 0))),
        std::make_tuple(
            std::vector<WeeklyTimeInterval>{
                WeeklyTimeInterval(
                    WeeklyTime(kTuesday, 10 * kMillisecondsInHour, 0),
                    WeeklyTime(kWednesday, 8 * kMillisecondsInHour, 0)),
                WeeklyTimeInterval(
                    WeeklyTime(kSunday, 5 * kMillisecondsInHour, 0),
                    WeeklyTime(kWednesday, 16 * kMillisecondsInHour, 0))},
            WeeklyTimeInterval(
                WeeklyTime(kSunday, 5 * kMillisecondsInHour, 0),
                WeeklyTime(kWednesday, 16 * kMillisecondsInHour, 0)))));

INSTANTIATE_TEST_CASE_P(
    DifferentTimezoneNone,
    GetIntervalForCurrentTimeTest,
    testing::Values(std::make_tuple(
        std::vector<WeeklyTimeInterval>{
            WeeklyTimeInterval(WeeklyTime(kTuesday,
                                          10 * kMillisecondsInHour,
                                          5 * kMillisecondsInHour),
                               WeeklyTime(kWednesday,
                                          17 * kMillisecondsInHour,
                                          5 * kMillisecondsInHour)),
            WeeklyTimeInterval(WeeklyTime(kSunday,
                                          5 * kMillisecondsInHour,
                                          5 * kMillisecondsInHour),
                               WeeklyTime(kSunday,
                                          16 * kMillisecondsInHour,
                                          5 * kMillisecondsInHour))},
        base::nullopt)));

INSTANTIATE_TEST_CASE_P(
    DifferentTimezoneResult,
    GetIntervalForCurrentTimeTest,
    testing::Values(std::make_tuple(
        std::vector<WeeklyTimeInterval>{
            WeeklyTimeInterval(WeeklyTime(kTuesday,
                                          10 * kMillisecondsInHour,
                                          -8 * kMillisecondsInHour),
                               WeeklyTime(kWednesday,
                                          8 * kMillisecondsInHour,
                                          -8 * kMillisecondsInHour)),
            WeeklyTimeInterval(WeeklyTime(kSunday,
                                          5 * kMillisecondsInHour,
                                          -8 * kMillisecondsInHour),
                               WeeklyTime(kSunday,
                                          16 * kMillisecondsInHour,
                                          -8 * kMillisecondsInHour))},
        WeeklyTimeInterval(WeeklyTime(kTuesday,
                                      10 * kMillisecondsInHour,
                                      -8 * kMillisecondsInHour),
                           WeeklyTime(kWednesday,
                                      8 * kMillisecondsInHour,
                                      -8 * kMillisecondsInHour)))));

}  // namespace weekly_time_utils
}  // namespace policy
