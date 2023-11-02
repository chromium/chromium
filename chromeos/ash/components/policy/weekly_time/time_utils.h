// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TIME_UTILS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class Clock;
}  // namespace base

namespace policy {

class WeeklyTime;
class WeeklyTimeInterval;

namespace weekly_time_utils {

// Put time in milliseconds which is added to local time to get GMT time to
// |offset| considering daylight from |clock|. Return true if there was no
// error.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool GetOffsetFromTimezoneToGmt(const std::string& timezone,
                                base::Clock* clock,
                                int* offset);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool GetOffsetFromTimezoneToGmt(const icu::TimeZone& timezone,
                                base::Clock* clock,
                                int* offset);

// The output is in the format "EEEE jj:mm a".
// Example: For a WeeklyTime(4 /* day_of_week */,
//                           5 * 3600*1000 /* milliseconds */,
//                           0 /* timezone_offset */)
// the output should be "Thursday 5:00 AM" in an US locale in GMT timezone.
// Similarly, the output will be "Donnerstag 05:00" in a German locale in a GMT
// timezone (there may be slight changes in formatting due to different
// standards in different locales).
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
std::u16string WeeklyTimeToLocalizedString(const WeeklyTime& weekly_time,
                                           base::Clock* clock);

// Convert time intervals from |timezone| to GMT timezone. Timezone agnostic
// intervals are not supported.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
std::vector<WeeklyTimeInterval> ConvertIntervalsToGmt(
    const std::vector<WeeklyTimeInterval>& intervals);

// Checks if |time| is contained in one of the |intervals|. Timezone agnostic
// intervals are not supported.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool Contains(const base::Time& time,
              const std::vector<WeeklyTimeInterval>& intervals);

// Returns next start or end interval time after |current_time|, or
// absl::nullopt in case |weekly_time_intervals| is empty.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
absl::optional<base::Time> GetNextEventTime(
    const base::Time& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals);

}  // namespace weekly_time_utils
}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
