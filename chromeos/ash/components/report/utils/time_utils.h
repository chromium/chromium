// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TIME_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TIME_UTILS_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}  // namespace base

namespace ash::report::utils {

// Converts the GMT time of the provided |clock| object to Pacific Time (PT)
// and returns a `base::Time` object.
// Conversion takes into account daylight saving time adjustments.
base::Time ConvertGmtToPt(base::Clock* clock);

// Return first UTC midnight of the previous month of |ts|.
absl::optional<base::Time> GetPreviousMonth(base::Time ts);

// Return first UTC midnight of the next month of |ts|.
absl::optional<base::Time> GetNextMonth(base::Time ts);

// Return the first UTC midnight of the previous year of |ts|.
absl::optional<base::Time> GetPreviousYear(base::Time ts);

// Return if |ts1| and |ts2| have the same month and year.
bool IsSameYearAndMonth(base::Time ts1, base::Time ts2);

// Formats |ts| as a GMT string in the format "YYYY-MM-DD 00:00:00.000 GMT".
std::string FormatTimestampToMidnightGMTString(base::Time ts);

// Convert the base::Time object to a string in YYYYMMDD format.
// Note: Date is based on UTC timezone, although we adjust ts to PST.
std::string TimeToYYYYMMDDString(base::Time ts);

// Convert the base::Time object to a string in YYYYMM format.
// Note: Date is based on UTC timezone, although we adjust ts to PST.
std::string TimeToYYYYMMString(base::Time ts);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_TIME_UTILS_H_
