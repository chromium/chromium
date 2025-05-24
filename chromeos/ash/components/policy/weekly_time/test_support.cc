// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/test_support.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/checked_util.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::weekly_time {

const char* DayToString(WeeklyTimeChecked::Day day_of_week) {
  int day_of_week_index = static_cast<int>(day_of_week) - 1;
  return WeeklyTimeChecked::kWeekDays[day_of_week_index];
}

base::Value::Dict BuildWeeklyTimeCheckedDict(WeeklyTimeChecked::Day day_of_week,
                                             int milliseconds_since_midnight) {
  base::Value::Dict dict;
  const char* day_of_week_str = DayToString(day_of_week);
  EXPECT_TRUE(dict.Set(WeeklyTimeChecked::kDayOfWeek, day_of_week_str));
  EXPECT_TRUE(dict.Set(WeeklyTimeChecked::kMillisecondsSinceMidnight,
                       milliseconds_since_midnight));
  return dict;
}

base::Value::Dict BuildWeeklyTimeCheckedDict(base::TimeDelta time_delta) {
  auto w = WeeklyTimeChecked::FromTimeDelta(time_delta);
  return BuildWeeklyTimeCheckedDict(w.day_of_week(),
                                    w.milliseconds_since_midnight());
}

base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(base::Value::Dict start,
                                                     base::Value::Dict end) {
  base::Value::Dict dict;
  EXPECT_TRUE(dict.Set(WeeklyTimeIntervalChecked::kStart, std::move(start)));
  EXPECT_TRUE(dict.Set(WeeklyTimeIntervalChecked::kEnd, std::move(end)));
  return dict;
}

base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(base::TimeDelta start,
                                                     base::TimeDelta end) {
  return BuildWeeklyTimeIntervalCheckedDict(BuildWeeklyTimeCheckedDict(start),
                                            BuildWeeklyTimeCheckedDict(end));
}

base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(
    WeeklyTimeChecked::Day start_day,
    int start_milliseconds,
    WeeklyTimeChecked::Day end_day,
    int end_milliseconds) {
  return BuildWeeklyTimeIntervalCheckedDict(
      BuildWeeklyTimeCheckedDict(start_day, start_milliseconds),
      BuildWeeklyTimeCheckedDict(end_day, end_milliseconds));
}

base::Value::List BuildList(std::string_view json_str) {
  std::optional<base::Value> value = base::JSONReader::Read(json_str);
  if (!value.has_value()) {
    ADD_FAILURE() << "JSON parsing failed: " << json_str;
    return {};
  }
  if (!value->is_list()) {
    ADD_FAILURE() << "JSON value not a list: " << json_str;
    return {};
  }
  return std::move(value.value()).TakeList();
}

base::Value::List BuildList(base::Time now,
                            base::TimeDelta from_now,
                            base::TimeDelta duration) {
  // TODO(isandrk): Can probably simplify function to only pass in start_time
  // (which would be equal to now + from_now).
  WeeklyTimeChecked current_weekly_time_checked =
      WeeklyTimeChecked::FromTimeAsLocalTime(now);
  base::TimeDelta current_time_of_week =
      current_weekly_time_checked.ToTimeDelta();
  base::TimeDelta start = current_time_of_week + from_now;
  base::TimeDelta end = start + duration;
  return base::Value::List().Append(
      BuildWeeklyTimeIntervalCheckedDict(start, end));
}

std::vector<WeeklyTimeIntervalChecked> BuildIntervals(
    std::string_view json_str) {
  base::Value::List list = BuildList(json_str);
  auto result = ExtractIntervalsFromList(list);
  if (!result.has_value()) {
    ADD_FAILURE() << "Couldn't parse intervals from list: "
                  << list.DebugString();
    return {};
  }
  return result.value();
}

base::Time TimeFromString(const char* str) {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString(str, &time));
  return time;
}

}  // namespace policy::weekly_time
