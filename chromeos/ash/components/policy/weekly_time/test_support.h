// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TEST_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TEST_SUPPORT_H_

#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"

namespace policy {

// Represents an interval point from
// `chromeos::prefs::kDeviceRestrictionSchedule`.
base::Value::Dict BuildWeeklyTimeCheckedDict(WeeklyTimeChecked::Day day_of_week,
                                             int milliseconds_since_midnight);

// Represents an interval from `chromeos::prefs::kDeviceRestrictionSchedule`.
base::Value::Dict BuildWeeklyTimeIntervalCheckedDict(
    WeeklyTimeChecked::Day start_day, int start_milliseconds,
    WeeklyTimeChecked::Day end_day, int end_milliseconds);

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TEST_SUPPORT_H_
