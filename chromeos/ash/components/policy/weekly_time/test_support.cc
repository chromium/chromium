// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/test_support.h"

#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

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

}  // namespace policy
