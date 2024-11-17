// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TEST_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TEST_SUPPORT_H_

#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"

namespace policy {
class WeeklyTimeIntervalChecked;
}

namespace policy::weekly_time {

// Converts a `WeeklyTimeChecked::Day` to its string representation.
const char* DayToString(WeeklyTimeChecked::Day day_of_week);

// Represents an interval point from
// `chromeos::prefs::kDeviceRestrictionSchedule`.
base::Value::Dict BuildWeeklyTimeCheckedDict(WeeklyTimeChecked::Day day_of_week,
                                             int milliseconds_since_midnight);
base::Value::Dict BuildWeeklyTimeCheckedDict(base::TimeDelta time_delta);

// Represents an interval from `chromeos::prefs::kDeviceRestrictionSchedule`.
base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(base::TimeDelta start,
                                                     base::TimeDelta end);
base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(base::Value::Dict start,
                                                     base::Value::Dict end);
base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(
    WeeklyTimeChecked::Day start_day, int start_milliseconds,
    WeeklyTimeChecked::Day end_day, int end_milliseconds);

// Builds a `base::Value::List` from the given `json_str`.
base::Value::List BuildList(std::string_view json_str);

// Builds a list with one interval starting `from_now` from `now` with duration
// `duration`. `from_now` can be negative meaning the interval started in the
// past.
base::Value::List BuildList(base::Time now,
                            base::TimeDelta from_now,
                            base::TimeDelta duration);

// Builds a list of intervals from the given `json_str`.
std::vector<WeeklyTimeIntervalChecked> BuildIntervals(
    std::string_view json_str);

// Creates a `base::Time` from the supplied `str`.
base::Time TimeFromString(const char* str);

}  // namespace policy::weekly_time

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TEST_SUPPORT_H_
