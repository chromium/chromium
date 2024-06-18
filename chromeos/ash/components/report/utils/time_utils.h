// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TIME_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TIME_UTILS_H_

#include <optional>

#include "base/time/time.h"

namespace base {
class Clock;
}  // namespace base

namespace ash::report::utils {

// day_of_week index for Monday in the base::Time::Exploded object.
static constexpr int kMondayDayOfWeekIndex = 1;
static constexpr int kThursdayDayOfWeekIndex = 4;

// Default value for devices that are missing the activate date.
static constexpr char kActivateDateKeyNotFound[] =
    "ACTIVATE_DATE_KEY_NOT_FOUND";

// Shared constants across reporting.
static constexpr int kNumberOfDaysInWeek = 7;
static constexpr int kMonthsInYear = 12;

// Converts the GMT time of the provided |clock| object to Pacific Time (PT)
// and returns a `base::Time` object.
// Conversion takes into account daylight saving time adjustments.
base::Time ConvertGmtToPt(base::Clock* clock);

// Return first UTC midnight of the previous month of |ts|.
std::optional<base::Time> GetPreviousMonth(base::Time ts);

// Return first UTC midnight of the next month of |ts|.
std::optional<base::Time> GetNextMonth(base::Time ts);

// Return the first UTC midnight of the previous year of |ts|.
std::optional<base::Time> GetPreviousYear(base::Time ts);

// Return if |ts1| and |ts2| have the same month and year.
bool IsSameYearAndMonth(base::Time ts1, base::Time ts2);

// Check if first active week is under num_days before active ts.
bool IsFirstActiveUnderNDaysAgo(base::Time active_ts,
                                base::Time first_active_week,
                                int num_days);

// Formats |ts| as a GMT string in the format "YYYY-MM-DD 00:00:00.000 GMT".
std::string FormatTimestampToMidnightGMTString(base::Time ts);

// Convert the base::Time object to a string in YYYYMMDD format.
// Note: Date is based on UTC timezone, although we adjust ts to PST.
std::string TimeToYYYYMMDDString(base::Time ts);

// Convert the base::Time object to a string in YYYYMM format.
// Note: Date is based on UTC timezone, although we adjust ts to PST.
std::string TimeToYYYYMMString(base::Time ts);

// Get 1st day of the GMT based first active week, similar to ISO8601 date
// (week) format. Field relies on ActivateDate VPD field, which is set after
// ash-chrome is restarted following the completion of OOBE for the first time.
std::optional<base::Time> GetFirstActiveWeek();

// Calculates the date of the first Monday similar to ISO calendar for a
// given year.
std::optional<base::Time> FirstMondayOfISONewYear(int iso_year);

// The ActivateDate is formatted: YYYY-WW and is generated based on GMT date.
// Return the first day of the ISO8601 week as a timestamp.
std::optional<base::Time> Iso8601DateWeekAsTime(int activate_year,
                                                int activate_week_of_year);

// Convert base::Time object to YYYY-WW similar to ISO8601.
// ISO calendar new year may start 1-3 days before the Gregorian new year or
// 1-3 days later.
// i.e. 2020-01 year by ISO calendar starts 2 days before, on December 30
// i.e. 2021-01 year by ISO calendar starts 3 days later, on January 4
std::string ConvertTimeToISO8601String(base::Time ts);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TIME_UTILS_H_
