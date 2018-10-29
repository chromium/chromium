// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
#define CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "chromeos/chromeos_export.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class Clock;
class TimeDelta;
}  // namespace base

namespace policy {

class WeeklyTime;
class WeeklyTimeInterval;

namespace weekly_time_utils {

// Put time in milliseconds which is added to local time to get GMT time to
// |offset| considering daylight from |clock|. Return true if there was no
// error.
CHROMEOS_EXPORT bool GetOffsetFromTimezoneToGmt(const std::string& timezone,
                                                base::Clock* clock,
                                                int* offset);

CHROMEOS_EXPORT bool GetOffsetFromTimezoneToGmt(const icu::TimeZone& timezone,
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
CHROMEOS_EXPORT base::string16 WeeklyTimeToLocalizedString(
    const WeeklyTime& weekly_time,
    base::Clock* clock);

// Convert time intervals from |timezone| to GMT timezone. Timezone agnostic
// intervals are not supported.
CHROMEOS_EXPORT std::vector<WeeklyTimeInterval> ConvertIntervalsToGmt(
    const std::vector<WeeklyTimeInterval>& intervals);

// Return duration till next weekly time interval.
CHROMEOS_EXPORT base::TimeDelta GetDeltaTillNextTimeInterval(
    const WeeklyTime& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals);

// Takes in a vector of weekly time intervals. If |clock->Now()|
// is inside one of the intervals, then the function returns the
// interval that contains |clock->Now()|. Otherwise, return |base::nullopt|.
// The intervals must have a defined
CHROMEOS_EXPORT base::Optional<WeeklyTimeInterval> GetIntervalForCurrentTime(
    const std::vector<WeeklyTimeInterval>& intervals,
    base::Clock* clock);

}  // namespace weekly_time_utils
}  // namespace policy

#endif  // CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
