// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_CHECKED_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_CHECKED_UTIL_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/values.h"

namespace policy {
class WeeklyTimeIntervalChecked;
class WeeklyTimeChecked;
}  // namespace policy

namespace policy::weekly_time {

// Extracts a list of intervals from a Value::List with format like
// `chromeos::prefs::kDeviceRestrictionSchedule`. Returns std::nullopt on error.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
std::optional<std::vector<WeeklyTimeIntervalChecked>> ExtractIntervalsFromList(
    const base::Value::List& list);

// Returns whether any of the intervals contain the given time.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool IntervalsContainTime(
    const std::vector<WeeklyTimeIntervalChecked>& intervals,
    const WeeklyTimeChecked& time);

// Returns a `WeeklyTimeChecked` that corresponds to the first next interval
// start or end time that comes after `time`, or std::nullopt in case intervals
// is empty.
// NB: If there's an event at the exact same time as `time`, it is ignored, and
// the one after that is used for the calculation.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
std::optional<WeeklyTimeChecked> GetNextEvent(
    const std::vector<WeeklyTimeIntervalChecked>& intervals,
    const WeeklyTimeChecked& time);

// Returns a TimeDelta from `time` until the first next interval start or end
// time, or std::nullopt in case intervals is empty.
// NB: If there's an event at the exact same time as `time`, it is ignored, and
// the one after that is used for the calculation (ie. the returned TimeDelta
// will always be positive).
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
std::optional<base::TimeDelta> GetDurationToNextEvent(
    const std::vector<WeeklyTimeIntervalChecked>& intervals,
    const WeeklyTimeChecked& time);

}  // namespace policy::weekly_time

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_CHECKED_UTIL_H_
