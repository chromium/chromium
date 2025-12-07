// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/time_utils.h"

#include <memory>
#include <string_view>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/time_utils.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash::report::utils {

namespace {

// Record histogram for whether ActivateDate is read and parsed correctly.
void RecordIsActivateDateSet(bool is_set) {
  base::UmaHistogramBoolean("Ash.Report.IsActivateDateSet", is_set);
}

}  // namespace

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

std::optional<base::Time> GetPreviousMonth(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp not set = " << ts;
    return std::nullopt;
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
    return std::nullopt;
  }

  return new_month_ts;
}

std::optional<base::Time> GetNextMonth(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp not set = " << ts;
    return std::nullopt;
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
    return std::nullopt;
  }

  return new_month_ts;
}

std::optional<base::Time> GetPreviousYear(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp not set = " << ts;
    return std::nullopt;
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
    return std::nullopt;
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

bool IsFirstActiveUnderNDaysAgo(base::Time active_ts,
                                base::Time first_active_week,
                                int num_days) {
  // Checks for the starting point which is num of days before active_ts.
  base::Time starting_point = active_ts - base::Days(num_days);

  // Check if first_active_week is after the starting point
  return first_active_week >= starting_point;
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

// The ActivateDate is formatted: YYYY-WW and is generated based on UTC date
// using `date +%Y-%W`.
// For week 00, returns Jan 1st of that year.
// For weeks 01-53, returns the Monday of that week.
std::optional<base::Time> VpdActivateDateWeekAsTime(int year,
                                                    int week_of_year) {
  if (year < 0 || week_of_year < 0 || week_of_year > 53) {
    LOG(ERROR) << "Invalid year or week of year"
               << ". Variable year = " << year
               << ". Variable week_of_year = " << week_of_year;
    return std::nullopt;
  }

  // Get the first day of the year.
  base::Time::Exploded jan1_exploded = {year, 1, 0, 1, 0, 0, 0, 0};
  base::Time jan1;
  if (!base::Time::FromUTCExploded(jan1_exploded, &jan1)) {
    LOG(ERROR) << "Failed to explode first day of year = " << year;
    return std::nullopt;
  }

  // Per `date +%W`, week 00 contains the days from Jan 1st until the first
  // Monday of the year. We will represent this week by its first day.
  if (week_of_year == 0) {
    return jan1;
  }

  // Get day of week for Jan 1st. (Sunday=0, Monday=1, ..)
  jan1.UTCExplode(&jan1_exploded);
  int jan1_day_of_week = jan1_exploded.day_of_week;

  // Get the first Monday of the year. Week 01 starts on this day.
  int days_to_first_monday = (1 - jan1_day_of_week + 7) % 7;
  base::Time first_monday_of_year = jan1 + base::Days(days_to_first_monday);

  // Week `week_of_year` starts `(week_of_year - 1)` weeks after the first
  // Monday.
  return first_monday_of_year + base::Days((week_of_year - 1) * 7);
}

std::optional<base::Time> GetFirstActiveWeek() {
  std::optional<std::string_view> first_active_week_val =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kActivateDateKey);
  std::string first_active_week_str =
      std::string(first_active_week_val.value_or(kActivateDateKeyNotFound));

  if (first_active_week_str == kActivateDateKeyNotFound) {
    LOG(ERROR)
        << "Failed to retrieve ActivateDate VPD field from machine statistics. "
        << "Leaving |first_active_week_| unset.";
    RecordIsActivateDateSet(false);
    return std::nullopt;
  }

  // Activate date is formatted: "YYYY-WW"
  int delimiter_index = first_active_week_str.find('-');

  const int expected_first_active_week_size = 7;
  const int expected_delimiter_index = 4;
  if (first_active_week_str.size() != expected_first_active_week_size ||
      delimiter_index != expected_delimiter_index) {
    LOG(ERROR) << "ActivateDate was retrieved but is not formatted as YYYY-WW. "
               << "Received string : " << first_active_week_str;
    RecordIsActivateDateSet(false);
    return std::nullopt;
  }

  const int expected_year_size = 4;
  const int expected_weeks_size = 2;

  std::string parsed_year = first_active_week_str.substr(0, expected_year_size);
  std::string parsed_weeks = first_active_week_str.substr(
      expected_delimiter_index + 1, expected_weeks_size);

  if (parsed_year.empty() || parsed_weeks.empty()) {
    LOG(ERROR) << "Failed to parse and convert the first active weeks string "
               << "year and weeks.";
    RecordIsActivateDateSet(false);
    return std::nullopt;
  }

  // Convert parsed year and weeks to int.
  int activate_year, activate_week_of_year;
  bool success_year = base::StringToInt(parsed_year, &activate_year);
  bool success_week = base::StringToInt(parsed_weeks, &activate_week_of_year);

  if (!success_year || !success_week) {
    LOG(ERROR) << "Failed to convert parsed_year or parsed_weeks: "
               << parsed_year << " and " << parsed_weeks;
    RecordIsActivateDateSet(false);
    return std::nullopt;
  }

  auto first_active_ts =
      VpdActivateDateWeekAsTime(activate_year, activate_week_of_year);
  if (!first_active_ts.has_value()) {
    LOG(ERROR) << "Failed to convert year and week of year as a timestamp.";
    RecordIsActivateDateSet(false);
    return std::nullopt;
  }

  RecordIsActivateDateSet(true);
  return first_active_ts.value();
}

std::optional<base::Time> FirstMondayOfISONewYear(int iso_year) {
  // 1. Get week of first Thursday in iso_year.
  // 2. Subtract days to get the first Monday.
  // ISO calendar new year may start 1-3 days before the
  // Gregorian new year or 1-3 days later.

  // Get week of the first Thursday in ISO year.
  base::Time first_thursday_ts;
  base::Time::Exploded first_thursday_exploded = {iso_year, 1, 0, 1,
                                                  0,        0, 0, 0};
  bool success =
      base::Time::FromUTCExploded(first_thursday_exploded, &first_thursday_ts);

  if (!success) {
    LOG(ERROR) << "Failed to explode first day of iso Year = " << iso_year;
    return std::nullopt;
  }

  // Re-create exploded object from first thursday timestamp.
  // This allows us to get an accurate day of week, so that we can
  // determine the first Thursday in iso_year.
  first_thursday_ts.UTCExplode(&first_thursday_exploded);

  // Adjust number of days to get to the first Thursday of year.
  while (first_thursday_exploded.day_of_week != kThursdayDayOfWeekIndex) {
    first_thursday_ts += base::Days(1);

    // Recalculate exploded object.
    first_thursday_ts.UTCExplode(&first_thursday_exploded);
  }

  base::Time first_monday_ts =
      first_thursday_ts -
      base::Days(kThursdayDayOfWeekIndex - kMondayDayOfWeekIndex);

  return first_monday_ts;
}

std::string ConvertTimeToISO8601String(base::Time ts) {
  if (ts.is_null()) {
    LOG(ERROR) << "Timestamp ts is not defined correctly. ts = " << ts;
    return std::string();
  }

  // Calculate the year of the given time
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  int activate_year = exploded.year;

  std::optional<base::Time> first_monday_iso_year =
      FirstMondayOfISONewYear(activate_year);

  if (!first_monday_iso_year.has_value()) {
    return std::string();
  }

  // Assign to the last ISO week of previous year. We use W52 for simplicity.
  // Some years have 53 weeks, but we simply use 52 to avoid complexity here.
  // This captures ts for up to the 3 days after the new year that are apart of
  // the previous year last ISO week.
  if (ts < first_monday_iso_year.value()) {
    return base::NumberToString(activate_year - 1) + "-52";
  }

  std::optional<base::Time> first_monday_iso_year_next =
      FirstMondayOfISONewYear(activate_year + 1);

  if (!first_monday_iso_year_next.has_value()) {
    return std::string();
  }

  // Assign to first ISO week of next year.
  // This captures ts for up to the 3 days before the new year that are apart of
  // the current year first ISO week.
  if (ts >= first_monday_iso_year_next.value()) {
    return base::NumberToString(activate_year + 1) + "-01";
  }

  // Calculate the number of days between the given time and the first
  // Monday of the year
  base::TimeDelta delta = ts - first_monday_iso_year.value();
  int days_difference = delta.InDays();

  // Calculate the ISO 8601 week number
  int activate_week_of_year = days_difference / 7 + 1;

  return base::NumberToString(activate_year) + "-" +
         (activate_week_of_year < 10 ? "0" : "") +
         base::NumberToString(activate_week_of_year);
}

}  // namespace ash::report::utils
