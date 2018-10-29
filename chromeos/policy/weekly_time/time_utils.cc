// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/policy/weekly_time/time_utils.h"

#include <algorithm>
#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "chromeos/policy/weekly_time/weekly_time_interval.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"

namespace policy {
namespace weekly_time_utils {
namespace {
constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);
const char kFormatWeekdayHourMinute[] = "EEEE jj:mm a";
}  // namespace

bool GetOffsetFromTimezoneToGmt(const std::string& timezone,
                                base::Clock* clock,
                                int* offset) {
  auto zone = base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(timezone)));
  if (*zone == icu::TimeZone::getUnknown()) {
    LOG(ERROR) << "Unsupported timezone: " << timezone;
    return false;
  }

  return GetOffsetFromTimezoneToGmt(*zone, clock, offset);
}

bool GetOffsetFromTimezoneToGmt(const icu::TimeZone& timezone,
                                base::Clock* clock,
                                int* offset) {
  // Time in milliseconds which is added to GMT to get local time.
  int gmt_offset = timezone.getRawOffset();
  // Time in milliseconds which is added to local standard time to get local
  // wall clock time.
  int dst_offset = timezone.getDSTSavings();
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar =
      std::make_unique<icu::GregorianCalendar>(timezone, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Gregorian calendar error = " << u_errorName(status);
    return false;
  }
  UDate cur_date = static_cast<UDate>(clock->Now().ToDoubleT() *
                                      base::Time::kMillisecondsPerSecond);
  status = U_ZERO_ERROR;
  gregorian_calendar->setTime(cur_date, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Gregorian calendar set time error = " << u_errorName(status);
    return false;
  }
  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Daylight time error = " << u_errorName(status);
    return false;
  }
  if (day_light)
    gmt_offset += dst_offset;
  // -|gmt_offset| is time which is added to local time to get GMT time.
  *offset = -gmt_offset;
  return true;
}

base::string16 WeeklyTimeToLocalizedString(const WeeklyTime& weekly_time,
                                           base::Clock* clock) {
  WeeklyTime result = weekly_time;
  if (!weekly_time.timezone_offset()) {
    // Get offset to convert the WeeklyTime
    int offset;
    auto local_time_zone = base::WrapUnique(icu::TimeZone::createDefault());
    if (!GetOffsetFromTimezoneToGmt(*local_time_zone, clock, &offset)) {
      LOG(ERROR) << "Unable to obtain offset for time agnostic timezone";
      return base::string16();
    }
    result = weekly_time.ConvertToCustomTimezone(-offset);
  }
  // Clock with the current time.
  WeeklyTime now_weekly_time = WeeklyTime::GetCurrentGmtWeeklyTime(clock);
  // Offset the current time so that its day of the week and time match
  // |day_of_week| and |milliseconds_|.
  base::Time offset_time =
      clock->Now() + now_weekly_time.GetDurationTo(result.ConvertToTimezone(0));
  return base::TimeFormatWithPattern(offset_time, kFormatWeekdayHourMinute);
}

std::vector<WeeklyTimeInterval> ConvertIntervalsToGmt(
    const std::vector<WeeklyTimeInterval>& intervals) {
  std::vector<WeeklyTimeInterval> gmt_intervals;
  for (const auto& interval : intervals) {
    auto gmt_start = interval.start().ConvertToTimezone(0);
    auto gmt_end = interval.end().ConvertToTimezone(0);
    gmt_intervals.push_back(WeeklyTimeInterval(gmt_start, gmt_end));
  }
  return gmt_intervals;
}

base::TimeDelta GetDeltaTillNextTimeInterval(
    const WeeklyTime& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals) {
  // Weekly intervals repeat every week, therefore the maximum duration till
  // next weekly interval is one week.
  base::TimeDelta till_next_interval = kWeek;
  for (const auto& interval : weekly_time_intervals) {
    till_next_interval = std::min(till_next_interval,
                                  current_time.GetDurationTo(interval.start()));
  }
  return till_next_interval;
}

base::Optional<WeeklyTimeInterval> GetIntervalForCurrentTime(
    const std::vector<WeeklyTimeInterval>& intervals,
    base::Clock* clock) {
  WeeklyTime weekly_time_now = WeeklyTime::GetCurrentGmtWeeklyTime(clock);
  for (const auto& interval : intervals) {
    if (interval.Contains(weekly_time_now)) {
      return interval;
    }
  }
  return base::nullopt;
}

}  // namespace weekly_time_utils
}  // namespace policy
