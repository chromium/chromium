// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/time_utils.h"

#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/time_utils.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash::report::utils {

base::Time ConvertGmtToPt(base::Clock* clock) {
  base::Time gmt_ts = clock->Now();
  DCHECK(gmt_ts != base::Time::UnixEpoch() && gmt_ts != base::Time())
      << "Invalid timestamp ts  = " << gmt_ts;

  int pt_offset;
  bool offset_success = policy::weekly_time_utils::GetOffsetFromTimezoneToGmt(
      "America/Los_Angeles", clock, &pt_offset);

  if (!offset_success) {
    LOG(ERROR) << "Failed to get offset for Pacific Time. "
               << "Returning UTC-8 timezone as default.";
    return gmt_ts - base::Hours(8);
  }

  return gmt_ts - base::Milliseconds(pt_offset);
}

absl::optional<base::Time> GetPreviousMonth(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp not set = " << ts;
    return absl::nullopt;
  }

  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the previous month.
  // "+ 11) % 12) + 1" wraps the month around if it goes outside 1..12.
  exploded.month = (((exploded.month - 1) + 11) % 12) + 1;
  exploded.year -= (exploded.month == 12);
  exploded.day_of_month = 1;
  exploded.hour = exploded.minute = exploded.second = exploded.millisecond = 0;

  base::Time new_month_ts;
  bool success = base::Time::FromUTCExploded(exploded, &new_month_ts);

  if (!success) {
    LOG(ERROR) << "Failed to get previous month of ts = " << ts;
    return absl::nullopt;
  }

  return new_month_ts;
}

absl::optional<base::Time> GetNextMonth(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp not set = " << ts;
    return absl::nullopt;
  }

  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the next month.
  // "+ 11) % 12) + 1" wraps the month around if it goes outside 1..12.
  exploded.month = (((exploded.month + 1) + 11) % 12) + 1;
  exploded.year += (exploded.month == 1);
  exploded.day_of_month = 1;
  exploded.hour = exploded.minute = exploded.second = exploded.millisecond = 0;

  base::Time new_month_ts;
  bool success = base::Time::FromUTCExploded(exploded, &new_month_ts);

  if (!success) {
    LOG(ERROR) << "Failed to get next month of ts = " << ts;
    return absl::nullopt;
  }

  return new_month_ts;
}

absl::optional<base::Time> GetPreviousYear(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp not set = " << ts;
    return absl::nullopt;
  }

  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the previous year.
  exploded.year -= 1;
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time new_year_ts;
  bool success = base::Time::FromUTCExploded(exploded, &new_year_ts);

  if (!success) {
    LOG(ERROR) << "Failed to get previous year of ts = " << ts;
    return absl::nullopt;
  }

  return new_year_ts;
}

bool IsSameYearAndMonth(base::Time ts1, base::Time ts2) {
  base::Time::Exploded ts1_exploded;
  ts1.UTCExplode(&ts1_exploded);
  base::Time::Exploded ts2_exploded;
  ts2.UTCExplode(&ts2_exploded);
  return (ts1_exploded.year == ts2_exploded.year) &&
         (ts1_exploded.month == ts2_exploded.month);
}

std::string FormatTimestampToMidnightGMTString(base::Time ts) {
  return base::UnlocalizedTimeFormatWithPattern(ts, "yyyy-MM-dd 00:00:00.000 z",
                                                icu::TimeZone::getGMT());
}

std::string TimeToYYYYMMDDString(base::Time ts) {
  return base::UnlocalizedTimeFormatWithPattern(ts, "yyyyMMdd",
                                                icu::TimeZone::getGMT());
}

std::string TimeToYYYYMMString(base::Time ts) {
  return base::UnlocalizedTimeFormatWithPattern(ts, "yyyyMM",
                                                icu::TimeZone::getGMT());
}

}  // namespace ash::report::utils
