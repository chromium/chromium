// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"

#include <tuple>
#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

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

const int kMinutesInHour = 60;

constexpr base::TimeDelta kMinute = base::Minutes(1);

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

}  // namespace

class SingleWeeklyTimeIntervalTest
    : public testing::TestWithParam<std::tuple<int, int, int, int>> {
 protected:
  int start_day_of_week() const { return std::get<0>(GetParam()); }
  int start_time() const { return std::get<1>(GetParam()); }
  int end_day_of_week() const { return std::get<2>(GetParam()); }
  int end_time() const { return std::get<3>(GetParam()); }
};

TEST_P(SingleWeeklyTimeIntervalTest, Constructor) {
  WeeklyTime start = WeeklyTime(start_day_of_week(),
                                start_time() * kMinute.InMilliseconds(), 0);
  WeeklyTime end =
      WeeklyTime(end_day_of_week(), end_time() * kMinute.InMilliseconds(), 0);
  WeeklyTimeInterval interval = WeeklyTimeInterval(start, end);
  EXPECT_EQ(interval.start().day_of_week(), start_day_of_week());
  EXPECT_EQ(interval.start().milliseconds(),
            start_time() * kMinute.InMilliseconds());
  EXPECT_EQ(interval.end().day_of_week(), end_day_of_week());
  EXPECT_EQ(interval.end().milliseconds(),
            end_time() * kMinute.InMilliseconds());
  EXPECT_EQ(interval.start().timezone_offset(),
            interval.end().timezone_offset());
  EXPECT_EQ(interval.start().timezone_offset(), 0);
}

TEST_P(SingleWeeklyTimeIntervalTest, ToValue) {
  WeeklyTime start = WeeklyTime(start_day_of_week(), start_time(), 0);
  WeeklyTime end = WeeklyTime(end_day_of_week(), end_time(), 0);
  WeeklyTimeInterval interval = WeeklyTimeInterval(start, end);
  base::Value expected_interval_value(base::Value::Type::DICT);
  base::Value::Dict& dict = expected_interval_value.GetDict();
  dict.Set(WeeklyTimeInterval::kStart, start.ToValue());
  dict.Set(WeeklyTimeInterval::kEnd, end.ToValue());

  EXPECT_EQ(interval.ToValue(), expected_interval_value);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromProto_Empty) {
  em::WeeklyTimeIntervalProto interval_proto;
  auto result = WeeklyTimeInterval::ExtractFromProto(interval_proto, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromProto_NoEnd) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  start->set_day_of_week(kWeekdays[start_day_of_week()]);
  start->set_time(start_time());
  auto result = WeeklyTimeInterval::ExtractFromProto(interval_proto, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromProto_NoStart) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  end->set_day_of_week(kWeekdays[end_day_of_week()]);
  end->set_time(end_time());
  auto result = WeeklyTimeInterval::ExtractFromProto(interval_proto, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromProto_InvalidStart) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[0]);
  start->set_time(start_time());
  end->set_day_of_week(kWeekdays[end_day_of_week()]);
  end->set_time(end_time());
  auto result = WeeklyTimeInterval::ExtractFromProto(interval_proto, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromProto_InvalidEnd) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[start_day_of_week()]);
  start->set_time(start_time());
  end->set_day_of_week(kWeekdays[0]);
  end->set_time(end_time());
  auto result = WeeklyTimeInterval::ExtractFromProto(interval_proto, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromProto_Valid) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[start_day_of_week()]);
  start->set_time(start_time());
  end->set_day_of_week(kWeekdays[end_day_of_week()]);
  end->set_time(end_time());
  auto result = WeeklyTimeInterval::ExtractFromProto(interval_proto, 0);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->start().day_of_week(), start_day_of_week());
  EXPECT_EQ(result->start().milliseconds(), start_time());
  EXPECT_EQ(result->end().day_of_week(), end_day_of_week());
  EXPECT_EQ(result->end().milliseconds(), end_time());
  EXPECT_EQ(result->start().timezone_offset(), 0);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromDict_Empty) {
  base::Value::Dict dict;
  auto result = WeeklyTimeInterval::ExtractFromDict(dict, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromDict_NoEnd) {
  base::Value::Dict dict;
  base::Value::Dict start;
  EXPECT_TRUE(start.Set(WeeklyTime::kDayOfWeek,
                        WeeklyTime::kWeekDays[start_day_of_week()]));
  EXPECT_TRUE(start.Set(WeeklyTime::kTime, start_time()));
  dict.Set(WeeklyTimeInterval::kStart, std::move(start));

  auto result = WeeklyTimeInterval::ExtractFromDict(dict, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromDict_NoStart) {
  base::Value::Dict dict;
  base::Value::Dict end;
  EXPECT_TRUE(end.Set(WeeklyTime::kDayOfWeek,
                      WeeklyTime::kWeekDays[end_day_of_week()]));
  EXPECT_TRUE(end.Set(WeeklyTime::kTime, end_time()));
  dict.Set(WeeklyTimeInterval::kEnd, std::move(end));

  auto result = WeeklyTimeInterval::ExtractFromDict(dict, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromDict_InvalidStart) {
  base::Value::Dict dict;
  base::Value::Dict start;
  EXPECT_TRUE(start.Set(WeeklyTime::kDayOfWeek, WeeklyTime::kWeekDays[0]));
  EXPECT_TRUE(start.Set(WeeklyTime::kTime, start_time()));
  dict.Set(WeeklyTimeInterval::kStart, std::move(start));
  base::Value::Dict end;
  EXPECT_TRUE(end.Set(WeeklyTime::kDayOfWeek,
                      WeeklyTime::kWeekDays[end_day_of_week()]));
  EXPECT_TRUE(end.Set(WeeklyTime::kTime, end_time()));
  dict.Set(WeeklyTimeInterval::kEnd, std::move(end));

  auto result = WeeklyTimeInterval::ExtractFromDict(dict, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromDict_InvalidEnd) {
  base::Value::Dict dict;
  base::Value::Dict start;
  EXPECT_TRUE(start.Set(WeeklyTime::kDayOfWeek,
                        WeeklyTime::kWeekDays[start_day_of_week()]));
  EXPECT_TRUE(start.Set(WeeklyTime::kTime, start_time()));
  dict.Set(WeeklyTimeInterval::kStart, std::move(start));
  base::Value::Dict end;
  EXPECT_TRUE(end.Set(WeeklyTime::kDayOfWeek, WeeklyTime::kWeekDays[0]));
  EXPECT_TRUE(end.Set(WeeklyTime::kTime, end_time()));
  dict.Set(WeeklyTimeInterval::kEnd, std::move(end));

  auto result = WeeklyTimeInterval::ExtractFromDict(dict, 0);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeIntervalTest, ExtractFromDict_Valid) {
  base::Value::Dict dict;
  base::Value::Dict start;
  EXPECT_TRUE(start.Set(WeeklyTime::kDayOfWeek,
                        WeeklyTime::kWeekDays[start_day_of_week()]));
  EXPECT_TRUE(start.Set(WeeklyTime::kTime, start_time()));
  dict.Set(WeeklyTimeInterval::kStart, std::move(start));
  base::Value::Dict end;
  EXPECT_TRUE(end.Set(WeeklyTime::kDayOfWeek,
                      WeeklyTime::kWeekDays[end_day_of_week()]));
  EXPECT_TRUE(end.Set(WeeklyTime::kTime, end_time()));
  dict.Set(WeeklyTimeInterval::kEnd, std::move(end));

  auto result = WeeklyTimeInterval::ExtractFromDict(dict, 0);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->start().day_of_week(), start_day_of_week());
  EXPECT_EQ(result->start().milliseconds(), start_time());
  EXPECT_EQ(result->end().day_of_week(), end_day_of_week());
  EXPECT_EQ(result->end().milliseconds(), end_time());
  EXPECT_EQ(result->start().timezone_offset(), 0);
}

INSTANTIATE_TEST_SUITE_P(OneMinuteInterval,
                         SingleWeeklyTimeIntervalTest,
                         testing::Values(std::make_tuple(kWednesday,
                                                         kMinutesInHour,
                                                         kWednesday,
                                                         kMinutesInHour + 1)));
INSTANTIATE_TEST_SUITE_P(
    TheLongestInterval,
    SingleWeeklyTimeIntervalTest,
    testing::Values(
        std::make_tuple(kMonday, 0, kSunday, 24 * kMinutesInHour - 1)));

INSTANTIATE_TEST_SUITE_P(RandomInterval,
                         SingleWeeklyTimeIntervalTest,
                         testing::Values(std::make_tuple(kTuesday,
                                                         10 * kMinutesInHour,
                                                         kFriday,
                                                         14 * kMinutesInHour +
                                                             15)));

class WeeklyTimeIntervalAndWeeklyTimeTest
    : public testing::TestWithParam<
          std::tuple<int, int, int, int, int, int, bool>> {
 protected:
  int start_day_of_week() const { return std::get<0>(GetParam()); }
  int start_time() const { return std::get<1>(GetParam()); }
  int end_day_of_week() const { return std::get<2>(GetParam()); }
  int end_time() const { return std::get<3>(GetParam()); }
  int current_day_of_week() const { return std::get<4>(GetParam()); }
  int current_time() const { return std::get<5>(GetParam()); }
  bool expected_contains() const { return std::get<6>(GetParam()); }
};

TEST_P(WeeklyTimeIntervalAndWeeklyTimeTest, Contains) {
  WeeklyTime start = WeeklyTime(start_day_of_week(),
                                start_time() * kMinute.InMilliseconds(), 0);
  WeeklyTime end =
      WeeklyTime(end_day_of_week(), end_time() * kMinute.InMilliseconds(), 0);
  WeeklyTimeInterval interval = WeeklyTimeInterval(start, end);
  WeeklyTime weekly_time = WeeklyTime(
      current_day_of_week(), current_time() * kMinute.InMilliseconds(), 0);
  EXPECT_EQ(interval.Contains(weekly_time), expected_contains());
}

INSTANTIATE_TEST_SUITE_P(
    TheLongestInterval,
    WeeklyTimeIntervalAndWeeklyTimeTest,
    testing::Values(std::make_tuple(kMonday,
                                    0,
                                    kSunday,
                                    24 * kMinutesInHour - 1,
                                    kWednesday,
                                    10 * kMinutesInHour,
                                    true),
                    std::make_tuple(kSunday,
                                    24 * kMinutesInHour - 1,
                                    kMonday,
                                    0,
                                    kWednesday,
                                    10 * kMinutesInHour,
                                    false)));

INSTANTIATE_TEST_SUITE_P(
    TheShortestInterval,
    WeeklyTimeIntervalAndWeeklyTimeTest,
    testing::Values(std::make_tuple(kMonday,
                                    0,
                                    kMonday,
                                    1,
                                    kTuesday,
                                    9 * kMinutesInHour,
                                    false),
                    std::make_tuple(kMonday, 0, kMonday, 1, kMonday, 1, false),
                    std::make_tuple(kMonday, 0, kMonday, 1, kMonday, 0, true)));

INSTANTIATE_TEST_SUITE_P(
    CheckStartInterval,
    WeeklyTimeIntervalAndWeeklyTimeTest,
    testing::Values(std::make_tuple(kTuesday,
                                    10 * kMinutesInHour + 30,
                                    kFriday,
                                    14 * kMinutesInHour + 45,
                                    kTuesday,
                                    10 * kMinutesInHour + 30,
                                    true)));

INSTANTIATE_TEST_SUITE_P(
    CheckEndInterval,
    WeeklyTimeIntervalAndWeeklyTimeTest,
    testing::Values(std::make_tuple(kTuesday,
                                    10 * kMinutesInHour + 30,
                                    kFriday,
                                    14 * kMinutesInHour + 45,
                                    kFriday,
                                    14 * kMinutesInHour + 45,
                                    false)));

INSTANTIATE_TEST_SUITE_P(RandomInterval,
                         WeeklyTimeIntervalAndWeeklyTimeTest,
                         testing::Values(std::make_tuple(kFriday,
                                                         17 * kMinutesInHour +
                                                             60,
                                                         kMonday,
                                                         9 * kMinutesInHour,
                                                         kSunday,
                                                         14 * kMinutesInHour,
                                                         true),
                                         std::make_tuple(kMonday,
                                                         9 * kMinutesInHour,
                                                         kFriday,
                                                         17 * kMinutesInHour,
                                                         kSunday,
                                                         14 * kMinutesInHour,
                                                         false)));

}  // namespace policy
