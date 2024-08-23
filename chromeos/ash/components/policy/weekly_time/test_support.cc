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

base::Value::Dict BuildWeeklyTimeCheckedDict(WeeklyTimeChecked::Day day_of_week,
                                             int milliseconds_since_midnight) {
  base::Value::Dict dict;
  int day_of_week_index = static_cast<int>(day_of_week) - 1;
  const char* day_of_week_str = WeeklyTimeChecked::kWeekDays[day_of_week_index];
  EXPECT_TRUE(dict.Set(WeeklyTimeChecked::kDayOfWeek, day_of_week_str));
  EXPECT_TRUE(dict.Set(WeeklyTimeChecked::kMillisecondsSinceMidnight,
                       milliseconds_since_midnight));
  return dict;
}

base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(
    WeeklyTimeChecked::Day start_day, int start_milliseconds,
    WeeklyTimeChecked::Day end_day, int end_milliseconds) {
  base::Value::Dict dict;
  auto start = BuildWeeklyTimeCheckedDict(start_day, start_milliseconds);
  auto end = BuildWeeklyTimeCheckedDict(end_day, end_milliseconds);
  EXPECT_TRUE(dict.Set(WeeklyTimeIntervalChecked::kStart, std::move(start)));
  EXPECT_TRUE(dict.Set(WeeklyTimeIntervalChecked::kEnd, std::move(end)));
  return dict;
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
