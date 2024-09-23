// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/time_utils.h"

#include <algorithm>
#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"

namespace policy {
namespace weekly_time_utils {

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
  UDate cur_date = static_cast<UDate>(clock->Now().InSecondsFSinceUnixEpoch() *
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

bool Contains(const base::Time& time,
              const std::vector<WeeklyTimeInterval>& intervals) {
  WeeklyTime weekly_time = WeeklyTime::GetGmtWeeklyTime(time);
  for (const auto& interval : intervals) {
    DCHECK(interval.start().timezone_offset().has_value());
    if (interval.Contains(weekly_time))
      return true;
  }
  return false;
}

std::optional<base::Time> GetNextEventTime(
    const base::Time& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals) {
  if (weekly_time_intervals.empty())
    return std::nullopt;

  base::Time::Exploded exploded;
  current_time.UTCExplode(&exploded);
  const auto weekly_time = GetWeeklyTimeFromExploded(exploded, 0);

  // Weekly intervals repeat every week, therefore the maximum duration till
  // next weekly interval is one week.
  base::TimeDelta till_next_event = base::Days(7);
  for (const auto& interval : weekly_time_intervals) {
    if (weekly_time != interval.start())
      till_next_event = std::min(till_next_event,
                                 weekly_time.GetDurationTo(interval.start()));
    if (weekly_time != interval.end())
      till_next_event =
          std::min(till_next_event, weekly_time.GetDurationTo(interval.end()));
  }

  // base::Time has microseconds precision.
  // base::Time::Exploded and WeeklyTime have milliseconds precision.
  // By constructing |rounded_time| from |exploded|, we are adjusting the
  // precision to return the exact time.
  base::Time rounded_time;
  if (base::Time::FromUTCExploded(exploded, &rounded_time)) {
    return rounded_time + till_next_event;
  }

  // This is possible if FromUTCExploded fails during daylight saving time
  // switches, see base::Time::Midnight implementation.
  return current_time + till_next_event;
}

}  // namespace weekly_time_utils
}  // namespace policy
