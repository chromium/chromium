// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/policy/weekly_time/weekly_time.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace em = enterprise_management;

namespace policy {

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

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

constexpr int kMinutesInHour = 60;
constexpr int kMillisecondsInHour = 3600000;
constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);

}  // namespace

class SingleWeeklyTimeTest
    : public testing::TestWithParam<std::tuple<int, int, base::Optional<int>>> {
 public:
  int day_of_week() const { return std::get<0>(GetParam()); }
  int minutes() const { return std::get<1>(GetParam()); }
  base::Optional<int> timezone_offset() const {
    return std::get<2>(GetParam());
  }
};

TEST_P(SingleWeeklyTimeTest, Constructor) {
  WeeklyTime weekly_time = WeeklyTime(
      day_of_week(), minutes() * kMinute.InMilliseconds(), timezone_offset());
  EXPECT_EQ(weekly_time.day_of_week(), day_of_week());
  EXPECT_EQ(weekly_time.milliseconds(), minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(weekly_time.timezone_offset(), timezone_offset());
}

TEST_P(SingleWeeklyTimeTest, ToValue) {
  WeeklyTime weekly_time = WeeklyTime(
      day_of_week(), minutes() * kMinute.InMilliseconds(), timezone_offset());
  std::unique_ptr<base::DictionaryValue> weekly_time_value =
      weekly_time.ToValue();
  base::DictionaryValue expected_weekly_time;
  expected_weekly_time.SetInteger("day_of_week", day_of_week());
  expected_weekly_time.SetInteger("time", minutes() * kMinute.InMilliseconds());
  if (timezone_offset()) {
    expected_weekly_time.SetInteger("timezone_offset",
                                    timezone_offset().value());
  }
  EXPECT_EQ(*weekly_time_value, expected_weekly_time);
}

TEST_P(SingleWeeklyTimeTest, ExtractFromProto_InvalidDay) {
  int milliseconds = minutes() * kMinute.InMilliseconds();
  em::WeeklyTimeProto proto;
  proto.set_day_of_week(kWeekdays[0]);
  proto.set_time(milliseconds);
  auto result = WeeklyTime::ExtractFromProto(proto, timezone_offset());
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeTest, ExtractFromProto_InvalidTime) {
  em::WeeklyTimeProto proto;
  proto.set_day_of_week(kWeekdays[day_of_week()]);
  proto.set_time(-1);
  auto result = WeeklyTime::ExtractFromProto(proto, timezone_offset());
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeTest, ExtractFromProto_Valid) {
  int milliseconds = minutes() * kMinute.InMilliseconds();
  em::WeeklyTimeProto proto;
  proto.set_day_of_week(kWeekdays[day_of_week()]);
  proto.set_time(milliseconds);
  auto result = WeeklyTime::ExtractFromProto(proto, timezone_offset());
  ASSERT_TRUE(result);
  EXPECT_EQ(result->day_of_week(), day_of_week());
  EXPECT_EQ(result->milliseconds(), milliseconds);
  EXPECT_EQ(result->timezone_offset(), timezone_offset());
}

INSTANTIATE_TEST_CASE_P(
    TheSmallestCase,
    SingleWeeklyTimeTest,
    testing::Values(std::make_tuple(kMonday, 0, base::nullopt)));

INSTANTIATE_TEST_CASE_P(
    TheBiggestCase,
    SingleWeeklyTimeTest,
    testing::Values(std::make_tuple(kSunday, 24 * kMinutesInHour - 1, 0)));

INSTANTIATE_TEST_CASE_P(
    RandomCase,
    SingleWeeklyTimeTest,
    testing::Values(std::make_tuple(kWednesday, 15 * kMinutesInHour + 30, 10)));

class TwoWeeklyTimesAndDurationTest
    : public testing::TestWithParam<
          std::tuple<int, int, int, int, base::TimeDelta>> {
 public:
  int day1() const { return std::get<0>(GetParam()); }
  int minutes1() const { return std::get<1>(GetParam()); }
  int day2() const { return std::get<2>(GetParam()); }
  int minutes2() const { return std::get<3>(GetParam()); }
  base::TimeDelta expected_duration() const { return std::get<4>(GetParam()); }
};

TEST_P(TwoWeeklyTimesAndDurationTest, GetDuration) {
  WeeklyTime weekly_time1 =
      WeeklyTime(day1(), minutes1() * kMinute.InMilliseconds(), 0);
  WeeklyTime weekly_time2 =
      WeeklyTime(day2(), minutes2() * kMinute.InMilliseconds(), 0);
  EXPECT_EQ(weekly_time1.GetDurationTo(weekly_time2), expected_duration());
}

INSTANTIATE_TEST_CASE_P(ZeroDuration,
                        TwoWeeklyTimesAndDurationTest,
                        testing::Values(std::make_tuple(kWednesday,
                                                        kMinutesInHour,
                                                        kWednesday,
                                                        kMinutesInHour,
                                                        base::TimeDelta())));

INSTANTIATE_TEST_CASE_P(TheLongestDuration,
                        TwoWeeklyTimesAndDurationTest,
                        testing::Values(std::make_tuple(kMonday,
                                                        0,
                                                        kSunday,
                                                        24 * kMinutesInHour - 1,
                                                        kWeek - kMinute)));

INSTANTIATE_TEST_CASE_P(
    DifferentDurations,
    TwoWeeklyTimesAndDurationTest,
    testing::Values(
        std::make_tuple(kThursday, 54, kThursday, kMinutesInHour + 54, kHour),
        std::make_tuple(kSunday, 24 * kMinutesInHour - 1, kMonday, 0, kMinute),
        std::make_tuple(kSaturday,
                        15 * kMinutesInHour + 30,
                        kFriday,
                        17 * kMinutesInHour + 45,
                        base::TimeDelta::FromDays(6) +
                            base::TimeDelta::FromHours(2) +
                            base::TimeDelta::FromMinutes(15))));

class TwoWeeklyTimesAndDurationInDifferentTimezonesTest
    : public testing::TestWithParam<std::tuple<int,
                                               int,
                                               base::Optional<int>,
                                               int,
                                               int,
                                               base::Optional<int>,
                                               base::TimeDelta>> {
 public:
  int day1() const { return std::get<0>(GetParam()); }
  int minutes1() const { return std::get<1>(GetParam()); }
  base::Optional<int> offset1() const { return std::get<2>(GetParam()); }
  int day2() const { return std::get<3>(GetParam()); }
  int minutes2() const { return std::get<4>(GetParam()); }
  base::Optional<int> offset2() const { return std::get<5>(GetParam()); }
  base::TimeDelta expected_duration() const { return std::get<6>(GetParam()); }
};

TEST_P(TwoWeeklyTimesAndDurationInDifferentTimezonesTest, ConvertToTimezone) {
  WeeklyTime weekly_time1 =
      WeeklyTime(day1(), minutes1() * kMinute.InMilliseconds(), offset1());
  WeeklyTime weekly_time2 =
      WeeklyTime(day2(), minutes2() * kMinute.InMilliseconds(), offset2());
  EXPECT_EQ(weekly_time1.GetDurationTo(weekly_time2), expected_duration());
}

INSTANTIATE_TEST_CASE_P(
    DifferentTimezones,
    TwoWeeklyTimesAndDurationInDifferentTimezonesTest,
    testing::Values(std::make_tuple(kMonday,
                                    kMinutesInHour,
                                    kMillisecondsInHour,
                                    kMonday,
                                    kMinutesInHour,
                                    5 * kMillisecondsInHour,
                                    kWeek - base::TimeDelta::FromHours(4))));

INSTANTIATE_TEST_CASE_P(
    TimezoneMakesDurationWrapAround,
    TwoWeeklyTimesAndDurationInDifferentTimezonesTest,
    testing::Values(std::make_tuple(kMonday,
                                    kMinutesInHour,
                                    5 * kMillisecondsInHour,
                                    kMonday,
                                    kMinutesInHour,
                                    4 * kMillisecondsInHour,
                                    base::TimeDelta::FromHours(1))));

INSTANTIATE_TEST_CASE_P(
    TwoAgnosticTimezones,
    TwoWeeklyTimesAndDurationInDifferentTimezonesTest,
    testing::Values(std::make_tuple(kMonday,
                                    10 * kMinutesInHour,
                                    base::nullopt,
                                    kTuesday,
                                    5 * kMinutesInHour,
                                    base::nullopt,
                                    base::TimeDelta::FromHours(19))));

class TwoWeeklyTimesAndOffsetTest
    : public testing::TestWithParam<std::tuple<int, int, int, int, int>> {
 public:
  int day_of_week() const { return std::get<0>(GetParam()); }
  int minutes() const { return std::get<1>(GetParam()); }
  int offset_minutes() const { return std::get<2>(GetParam()); }
  int expected_day() const { return std::get<3>(GetParam()); }
  int expected_minutes() const { return std::get<4>(GetParam()); }
};

TEST_P(TwoWeeklyTimesAndOffsetTest, AddMilliseconds) {
  WeeklyTime weekly_time =
      WeeklyTime(day_of_week(), minutes() * kMinute.InMilliseconds(), 10);
  WeeklyTime result_weekly_time =
      weekly_time.AddMilliseconds(offset_minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(result_weekly_time.day_of_week(), expected_day());
  EXPECT_EQ(result_weekly_time.milliseconds(),
            expected_minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(result_weekly_time.timezone_offset(),
            weekly_time.timezone_offset());
}

INSTANTIATE_TEST_CASE_P(
    ZeroOffset,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kTuesday,
                                    15 * kMinutesInHour + 30,
                                    0,
                                    kTuesday,
                                    15 * kMinutesInHour + 30)));

INSTANTIATE_TEST_CASE_P(
    TheSmallestOffset,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kWednesday,
                                    15 * kMinutesInHour + 30,
                                    -13 * kMinutesInHour,
                                    kWednesday,
                                    2 * kMinutesInHour + 30),
                    std::make_tuple(kMonday,
                                    9 * kMinutesInHour + 30,
                                    -13 * kMinutesInHour,
                                    kSunday,
                                    20 * kMinutesInHour + 30)));

INSTANTIATE_TEST_CASE_P(
    TheBiggestOffset,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kTuesday,
                                    10 * kMinutesInHour + 30,
                                    13 * kMinutesInHour,
                                    kTuesday,
                                    23 * kMinutesInHour + 30),
                    std::make_tuple(kSunday,
                                    21 * kMinutesInHour + 30,
                                    13 * kMinutesInHour,
                                    kMonday,
                                    10 * kMinutesInHour + 30)));

INSTANTIATE_TEST_CASE_P(
    DifferentOffsets,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kWednesday,
                                    10 * kMinutesInHour + 47,
                                    5 * kMinutesInHour + 30,
                                    kWednesday,
                                    16 * kMinutesInHour + 17),
                    std::make_tuple(kMonday,
                                    10 * kMinutesInHour + 47,
                                    6 * kMinutesInHour + 15,
                                    kMonday,
                                    17 * kMinutesInHour + 2),
                    std::make_tuple(kThursday,
                                    22 * kMinutesInHour + 24,
                                    -7 * kMinutesInHour,
                                    kThursday,
                                    15 * kMinutesInHour + 24)));

class WeeklyTimeTimezoneConversionTest
    : public testing::TestWithParam<std::tuple<int, int, int, int, int, int>> {
 public:
  int day() { return std::get<0>(GetParam()); }
  int minutes() { return std::get<1>(GetParam()); }
  int timezone_offset() { return std::get<2>(GetParam()); }
  int result_day() { return std::get<3>(GetParam()); }
  int result_minutes() { return std::get<4>(GetParam()); }
  int result_offset() { return std::get<5>(GetParam()); }
};

TEST_P(WeeklyTimeTimezoneConversionTest, ConvertToTimezone) {
  WeeklyTime weekly_time = WeeklyTime(
      day(), minutes() * kMinute.InMilliseconds(), timezone_offset());
  WeeklyTime result = weekly_time.ConvertToTimezone(result_offset());
  EXPECT_EQ(result.day_of_week(), result_day());
  EXPECT_EQ(result.milliseconds(), result_minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(result.timezone_offset().value(), result_offset());
}

INSTANTIATE_TEST_CASE_P(
    ConversionToABiggerTimezone,
    WeeklyTimeTimezoneConversionTest,
    testing::Values(std::make_tuple(kMonday,
                                    10 * kMinutesInHour,
                                    2 * kMillisecondsInHour,
                                    kMonday,
                                    15 * kMinutesInHour,
                                    7 * kMillisecondsInHour)));

INSTANTIATE_TEST_CASE_P(ConversionToASmallerTimezone,
                        WeeklyTimeTimezoneConversionTest,
                        testing::Values(std::make_tuple(kMonday,
                                                        10 * kMinutesInHour,
                                                        4 * kMillisecondsInHour,
                                                        kMonday,
                                                        6 * kMinutesInHour,
                                                        0)));

INSTANTIATE_TEST_CASE_P(
    ConversionToTheSameTimezone,
    WeeklyTimeTimezoneConversionTest,
    testing::Values(std::make_tuple(kMonday,
                                    10 * kMinutesInHour,
                                    4 * kMillisecondsInHour,
                                    kMonday,
                                    10 * kMinutesInHour,
                                    4 * kMillisecondsInHour)));

INSTANTIATE_TEST_CASE_P(
    ConversionToANegativeTimezone,
    WeeklyTimeTimezoneConversionTest,
    testing::Values(std::make_tuple(kMonday,
                                    15 * kMinutesInHour,
                                    2 * kMillisecondsInHour,
                                    kMonday,
                                    9 * kMinutesInHour,
                                    -4 * kMillisecondsInHour)));

}  // namespace policy
