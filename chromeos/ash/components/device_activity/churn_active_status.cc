// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_active_status.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace ash::device_activity {

namespace {

// We define the inception date as January 1st 2000 GMT.
// This value is used to keep track of the number of months since the
// inception date in the first 10 bits of the active status
// |value_|.
const char kActiveStatusInceptionDate[] = "2000-01-01 00:00:00 GMT";

// Default value for devices that are missing the activate date.
const char kActivateDateKeyNotFound[] = "ACTIVATE_DATE_KEY_NOT_FOUND";

// Number of days in a week.
constexpr int kNumberOfDaysInWeek = 7;

// day_of_week index for Monday in the base::Time::Exploded object.
constexpr int kMondayDayOfWeekIndex = 1;

base::Time GetFirstMondayFromNewYear(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Only the Year, Month, and Day fields should be set.
  // All other values such as hour, minute, second should be set to 0.
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  // day_of_week field is 0 based starting from Sunday.
  int days_to_first_monday;

  if (kMondayDayOfWeekIndex >= exploded.day_of_week) {
    // Difference in days between Sunday/Monday to Monday.
    days_to_first_monday = kMondayDayOfWeekIndex - exploded.day_of_week;
  } else {
    // Number of days to get to the next Monday from the current day_of_week.
    days_to_first_monday = kNumberOfDaysInWeek - (exploded.day_of_week - 1);
  }

  return ts + base::Days(days_to_first_monday);
}

// VPD field ActivateDate is based on GMT/UTC timezone.
// The ActivateDate vpd field value includes the weeks: %W
// Note: week number of year, with Monday as first day of week (00..53)
base::Time Iso8601DateWeekAsTime(int activate_year, int activate_week_of_year) {
  if (activate_year < 0 || activate_week_of_year < 0 ||
      activate_week_of_year > 53) {
    LOG(ERROR) << "Invalid year or week of year"
               << ". Variable activate_year = " << activate_year
               << ". Variable activate_week_of_year = "
               << activate_week_of_year;
    return base::Time();
  }

  // Get the first monday of the parsed activate date year as a base::Time
  // object. This will make it easier to add the required number of days to get
  // the start of the ISO 8601 week standard period.
  base::Time new_year_ts;
  std::string new_year_date =
      "Jan 01 00:00:00 GMT " + std::to_string(activate_year);
  bool success = base::Time::FromUTCString(new_year_date.c_str(), &new_year_ts);

  if (!success) {
    LOG(ERROR)
        << "Failed to store new year in base::Time using FromUTCString method.";
    return base::Time();
  }

  // ISO 8601 assigns the weeks to 0 if the stored date was
  // before the first monday of the year.
  // For example, the first monday of 2020 is Jan 6th, so devices that had
  // Activated between [Jan 1st, Jan 5th] have activate week of year set to 0.
  if (activate_week_of_year == 0) {
    return new_year_ts;
  }

  base::Time first_monday_of_year = GetFirstMondayFromNewYear(new_year_ts);

  // Get the number of days to the start of a ISO 8601 week standard period for
  // that year from the years first monday.
  // This is equal to (activate_week_of_year-1) * 7 days.
  int days_in_iso_period = 0;
  days_in_iso_period = (activate_week_of_year - 1) * 7;

  // Add the above two steps to get the start of a ISO 8601 week time.
  return first_monday_of_year + base::Days(days_in_iso_period);
}

}  // namespace

ChurnActiveStatus::ChurnActiveStatus() : ChurnActiveStatus(0) {}

ChurnActiveStatus::ChurnActiveStatus(int value)
    : value_(value),
      statistics_provider_(system::StatisticsProvider::GetInstance()) {
  SetFirstActiveWeek();
}

ChurnActiveStatus::~ChurnActiveStatus() = default;

int ChurnActiveStatus::GetValueAsInt() const {
  return static_cast<int>(value_.to_ulong());
}

absl::optional<std::bitset<ChurnActiveStatus::kChurnBitSize>>
ChurnActiveStatus::UpdateValue(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  int year = exploded.year;
  int month = exploded.month;

  // Calculate total number of months since January 2000 to current month.
  // e.g. Dec 2022 should return a total of 275 months.
  int new_months_from_inception =
      ((year - kInceptionYear) * kMonthsInYear) + (month - 1);

  int previous_months_from_inception = GetMonthsSinceInception();

  // Check if new_months_from_inception is greater than previous
  // months_from_inception which was stored in |value_|.
  if (new_months_from_inception <= previous_months_from_inception) {
    LOG(ERROR) << "Failed to update churn active status value_. "
               << "New months from inception is smaller than the previous "
                  "number of months from inception.";
    return absl::nullopt;
  }

  // Calculate new_active_months since we are in a new month.
  // Shift the 18 bits N to the left to represent the inactive months, and
  // set the last bit to 1 to mark this month as active.
  std::bitset<kActiveMonthsBitSize> new_active_months(GetActiveMonthBits());
  new_active_months <<=
      (new_months_from_inception - previous_months_from_inception);
  new_active_months |= 1;

  // Recreate |value_| formatted with first 10 bits representing months from
  // inception to current month. Followed by 18 bits representing last 18 months
  // of actives from current month.
  std::bitset<kChurnBitSize> updated_value(new_months_from_inception);

  updated_value <<= kActiveMonthsBitSize;
  updated_value |= static_cast<int>(new_active_months.to_ulong());

  value_ = updated_value;
  return updated_value;
}

void ChurnActiveStatus::InitializeValue(int value) {
  DCHECK_EQ(value_, 0);
  value_ = value;
}

const base::Time ChurnActiveStatus::GetInceptionMonth() const {
  base::Time inception_ts;
  bool success =
      base::Time::FromUTCString(kActiveStatusInceptionDate, &inception_ts);

  if (!success) {
    LOG(ERROR) << "Failed to convert kActiveStatusInceptionDate to timestamp "
               << "object.";
    return base::Time();
  }

  return inception_ts;
}

int ChurnActiveStatus::GetMonthsSinceInception() const {
  // Get first 10 bits of |value_| which represent the total months since
  // inception.
  std::string month_from_inception = value_.to_string().substr(
      kMonthsSinceInceptionBitOffset,
      kActiveMonthsBitOffset - kMonthsSinceInceptionBitOffset);

  return static_cast<int>(
      std::bitset<kMonthsSinceInceptionSize>(month_from_inception).to_ulong());
}

const base::Time ChurnActiveStatus::GetCurrentActiveMonth() const {
  DCHECK_GE(GetMonthsSinceInception(), 0);

  base::Time inception_ts = GetInceptionMonth();
  int months_from_inception = GetMonthsSinceInception();

  int years_from_inception = std::floor(months_from_inception / 12);
  int months_from_inception_remaining = months_from_inception % 12;

  base::Time::Exploded exploded;
  inception_ts.UTCExplode(&exploded);

  exploded.year += years_from_inception;
  exploded.month += months_from_inception_remaining;

  base::Time current_active_month_ts;
  bool success =
      base::Time::FromUTCExploded(exploded, &current_active_month_ts);

  if (!success) {
    LOG(ERROR) << "Failed to convert current active month back to timestamp.";
    return base::Time();
  }

  return current_active_month_ts;
}

int ChurnActiveStatus::GetActiveMonthBits() {
  // Get bits at [10, 28] of |value_| which represents record of 18 months of
  // actives from months since inception.
  std::string active_months = value_.to_string().substr(
      kActiveMonthsBitOffset, kChurnBitSize - kActiveMonthsBitOffset);

  return static_cast<int>(
      std::bitset<kActiveMonthsBitSize>(active_months).to_ulong());
}

void ChurnActiveStatus::SetValueForTesting(
    std::bitset<ChurnActiveStatus::kChurnBitSize> val) {
  value_ = val;
}

const base::Time ChurnActiveStatus::GetFirstActiveWeek() const {
  return first_active_week_;
}

base::Time ChurnActiveStatus::GetFirstActiveWeekForTesting(
    const std::string& year,
    const std::string& weeks) {
  // Convert parsed year and weeks to int.
  int activate_year;
  int activate_week_of_year;

  bool year_success = base::StringToInt(year, &activate_year);
  bool week_of_year_success = base::StringToInt(weeks, &activate_week_of_year);

  if (!year_success || !week_of_year_success) {
    LOG(ERROR) << "Failed to convert string value year or weeks to type int.";
    return base::Time();
  }

  return Iso8601DateWeekAsTime(activate_year, activate_week_of_year);
}

void ChurnActiveStatus::SetFirstActiveWeek() {
  // Retrieve ActivateDate vpd field from machine statistics object.
  // Default |first_active_week_| to kActivateDateKeyNotFound if retrieval
  // from machine statistics fails.
  const absl::optional<base::StringPiece> first_active_week_val =
      statistics_provider_->GetMachineStatistic(system::kActivateDateKey);
  std::string first_active_week_str =
      std::string(first_active_week_val.value_or(kActivateDateKeyNotFound));

  // TODO(hirthanan): Add UMA histogram to count set vs unset after
  // browser start.
  if (first_active_week_str == kActivateDateKeyNotFound) {
    LOG(ERROR)
        << "Failed to retrieve ActivateDate VPD field from machine statistics. "
        << "Leaving |first_active_week_| unset.";
    return;
  }

  // Activate date is formatted: "YYYY-WW"
  int delimiter_index = first_active_week_str.find('-');

  const int expected_first_active_week_size = 7;
  const int expected_delimiter_index = 4;
  if (first_active_week_str.size() != expected_first_active_week_size ||
      delimiter_index != expected_delimiter_index) {
    LOG(ERROR) << "ActivateDate was retrieved but is not formatted as YYYY-WW. "
               << "Received string : " << first_active_week_str;
    return;
  }

  const int expected_year_size = 4;
  const int expected_weeks_size = 2;

  std::string parsed_year = first_active_week_str.substr(0, expected_year_size);
  std::string parsed_weeks = first_active_week_str.substr(
      expected_delimiter_index + 1, expected_weeks_size);

  // Verify |first_active_week_| string parsed successfully for the
  // year and weeks.
  if (parsed_year.empty() || parsed_weeks.empty()) {
    LOG(ERROR) << "Failed to parse and convert the first active weeks string "
               << "year and weeks.";
    return;
  }

  // Convert parsed year and weeks to int.
  int activate_year;
  int activate_week_of_year;

  base::StringToInt(parsed_year, &activate_year);
  base::StringToInt(parsed_weeks, &activate_week_of_year);

  // Convert an ISO 8601 date week year and week of year to base::Time object.
  // The returned base::Time object represents the start of a new ISO 8601 week.
  // This will be based on the activate week that is stored by ActivateDate.
  first_active_week_ =
      Iso8601DateWeekAsTime(activate_year, activate_week_of_year);
}

}  // namespace ash::device_activity
