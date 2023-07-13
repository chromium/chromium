// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/churn/active_status.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"

namespace ash::report::device_metrics {

namespace {

// day_of_week index for Monday in the base::Time::Exploded object.
constexpr int kMondayDayOfWeekIndex = 1;

template <size_t N>
int ConvertBitsetToInteger(std::bitset<N> bitset) {
  return static_cast<int>(bitset.to_ulong());
}

template <size_t N>
std::bitset<N> ConvertIntegerToBitset(int val) {
  return std::bitset<N>(val);
}

}  // namespace

ActiveStatus::ActiveStatus(PrefService* local_state)
    : local_state_(local_state),
      statistics_provider_(system::StatisticsProvider::GetInstance()) {}

int ActiveStatus::GetValue() const {
  return local_state_->GetInteger(
      ash::report::prefs::kDeviceActiveLastKnownChurnActiveStatus);
}

void ActiveStatus::SetValue(int val) {
  return local_state_->SetInteger(
      ash::report::prefs::kDeviceActiveLastKnownChurnActiveStatus, val);
}

absl::optional<int> ActiveStatus::CalculateNewValue(base::Time ts) const {
  if (ts.is_null() || ts == base::Time::UnixEpoch()) {
    LOG(ERROR) << "Cannot calculate new value for invalid ts.";
    return absl::nullopt;
  }

  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  int year = exploded.year;
  int month = exploded.month;

  // Calculate total number of months since January 2000 to current month.
  // e.g. Dec 2022 should return a total of 275 months.
  int new_months_from_inception =
      ((year - kInceptionYear) * kMonthsInYear) + (month - 1);
  int previous_months_from_inception = GetMonthsSinceInception();

  // Check |ts| represents a new month than previously reported.
  if (new_months_from_inception <= previous_months_from_inception) {
    LOG(ERROR) << "Failed to update churn active status. "
               << "New number of months must be larger than the previous.";
    LOG(ERROR) << "Previous months from inception = "
               << previous_months_from_inception;
    LOG(ERROR) << "New months from inception = " << new_months_from_inception;

    return absl::nullopt;
  }

  // Calculate new_active_months since we are in a new month.
  // Shift the 18 bits N to the left to represent the inactive months, and
  // set the last bit to 1 to mark this month as active.
  std::bitset<kActiveMonthsBitSize> new_active_months(GetActiveMonthBits());
  new_active_months <<=
      (new_months_from_inception - previous_months_from_inception);
  new_active_months |= 1;

  // Recreate active status bitset formatted with first 10 bits representing
  // months from inception to current month. Followed by 18 bits representing
  // last 18 months of actives from current month.
  std::bitset<kActiveStatusBitSize> updated_value(new_months_from_inception);

  updated_value <<= kActiveMonthsBitSize;
  updated_value |= static_cast<int>(new_active_months.to_ulong());

  return ConvertBitsetToInteger<kActiveStatusBitSize>(updated_value);
}

absl::optional<base::Time> ActiveStatus::GetCurrentActiveMonthTimestamp()
    const {
  DCHECK_GE(GetMonthsSinceInception(), 0);

  int months_from_inception = GetMonthsSinceInception();
  absl::optional<base::Time> inception_ts = GetInceptionMonthTimestamp();
  if (!inception_ts.has_value()) {
    LOG(ERROR) << "Failed to get the inception month as timestamp.";
    return absl::nullopt;
  }

  int years_from_inception = std::floor(months_from_inception / 12);
  int months_from_inception_remaining = months_from_inception % 12;

  base::Time::Exploded exploded;
  inception_ts.value().UTCExplode(&exploded);

  exploded.year += years_from_inception;
  exploded.month += months_from_inception_remaining;

  base::Time current_active_month_ts;
  bool success =
      base::Time::FromUTCExploded(exploded, &current_active_month_ts);

  if (!success) {
    LOG(ERROR) << "Failed to convert current active month back to timestamp.";
    return absl::nullopt;
  }

  return current_active_month_ts;
}

absl::optional<ChurnCohortMetadata> ActiveStatus::CalculateCohortMetadata(
    base::Time active_ts) const {
  ChurnCohortMetadata metadata;

  absl::optional<int> new_active_status = CalculateNewValue(active_ts);
  if (!new_active_status.has_value()) {
    LOG(ERROR) << "Failed to generate new value. Old Value = " << GetValue();
    return absl::nullopt;
  }

  metadata.set_active_status_value(new_active_status.value());

  absl::optional<bool> is_first_active = IsFirstActiveInCohort(active_ts);
  if (is_first_active.has_value()) {
    metadata.set_is_first_active_in_cohort(is_first_active.value());
  }

  return metadata;
}

absl::optional<ChurnObservationMetadata>
ActiveStatus::CalculateObservationMetadata(base::Time active_ts,
                                           int period) const {
  DCHECK(period >= 0 && period <= 2) << "Period must be in [0,2] range.";

  // Observation metadata is generated if cohort ping was sent for the month.
  absl::optional<base::Time> cur_active_month_ts =
      GetCurrentActiveMonthTimestamp();
  if (cur_active_month_ts.has_value() &&
      !utils::IsSameYearAndMonth(cur_active_month_ts.value(), active_ts)) {
    LOG(ERROR) << "Observation metadata require a current active status value. "
               << "This occurs after successful cohort pinging.";
    return absl::nullopt;
  }

  ChurnObservationMetadata metadata;

  bool is_monthly_active = IsDeviceActiveInMonth(kMonthlyChurnOffset + period);
  bool is_yearly_active = IsDeviceActiveInMonth(kYearlyChurnOffset + period);
  metadata.set_monthly_active_status(is_monthly_active);
  metadata.set_yearly_active_status(is_yearly_active);

  absl::optional<base::Time> first_active_week = GetFirstActiveWeek();
  if (!first_active_week.has_value()) {
    LOG(ERROR) << "Cannot calculate observation metadata for first active "
               << "during cohort without the first active week.";
    return metadata;
  }

  absl::optional<base::Time> last_month_ts = utils::GetPreviousMonth(active_ts);
  absl::optional<base::Time> two_months_ago_ts =
      utils::GetPreviousMonth(last_month_ts.value_or(base::Time()));
  absl::optional<base::Time> three_months_ago_ts =
      utils::GetPreviousMonth(two_months_ago_ts.value_or(base::Time()));

  if (!last_month_ts.has_value() || !two_months_ago_ts.has_value() ||
      !three_months_ago_ts.has_value()) {
    LOG(ERROR) << "Failed to calculate observation metadata for period.";
    return absl::nullopt;
  }

  absl::optional<base::Time> month_before_observation_period_start_ts;
  absl::optional<base::Time> year_before_observation_period_start_ts;

  if (period == 0) {
    month_before_observation_period_start_ts = last_month_ts;
    year_before_observation_period_start_ts = utils::GetPreviousYear(
        month_before_observation_period_start_ts.value_or(base::Time()));

  } else if (period == 1) {
    month_before_observation_period_start_ts = two_months_ago_ts;
    year_before_observation_period_start_ts = utils::GetPreviousYear(
        month_before_observation_period_start_ts.value_or(base::Time()));

  } else if (period == 2) {
    month_before_observation_period_start_ts = three_months_ago_ts;
    year_before_observation_period_start_ts = utils::GetPreviousYear(
        month_before_observation_period_start_ts.value_or(base::Time()));
  }

  if (!month_before_observation_period_start_ts.has_value() ||
      !year_before_observation_period_start_ts.has_value()) {
    LOG(ERROR) << "Failed to get timestamps used to calculate first active "
                  "during cohort.";
    return metadata;
  }

  // Calculate the device's first active status in different cohort months.
  if (utils::IsSameYearAndMonth(
          first_active_week.value(),
          month_before_observation_period_start_ts.value()) &&
      is_monthly_active) {
    metadata.set_first_active_during_cohort(
        ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_MONTHLY_COHORT);
  } else if (utils::IsSameYearAndMonth(
                 first_active_week.value(),
                 year_before_observation_period_start_ts.value()) &&
             is_yearly_active) {
    metadata.set_first_active_during_cohort(
        ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_YEARLY_COHORT);
  } else {
    metadata.set_first_active_during_cohort(
        ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  }

  return metadata;
}

absl::optional<base::Time> ActiveStatus::GetInceptionMonthTimestamp() const {
  base::Time inception_ts;
  bool success = base::Time::FromUTCString(
      ActiveStatus::kActiveStatusInceptionDate, &inception_ts);

  if (!success) {
    LOG(ERROR) << "Failed to convert kActiveStatusInceptionDate to timestamp.";
    return absl::nullopt;
  }

  return inception_ts;
}

base::Time ActiveStatus::GetFirstMondayFromNewYear(base::Time ts) const {
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
    days_to_first_monday =
        ActiveStatus::kNumberOfDaysInWeek - (exploded.day_of_week - 1);
  }

  return ts + base::Days(days_to_first_monday);
}

// The ActivateDate is formatted: YYYY-WW and is generated based on UTC date.
// Returns the first day of the ISO8601 week.
absl::optional<base::Time> ActiveStatus::Iso8601DateWeekAsTime(
    int activate_year,
    int activate_week_of_year) const {
  if (activate_year < 0 || activate_week_of_year < 0 ||
      activate_week_of_year > 53) {
    LOG(ERROR) << "Invalid year or week of year"
               << ". Variable activate_year = " << activate_year
               << ". Variable activate_week_of_year = "
               << activate_week_of_year;
    return absl::nullopt;
  }

  // Get the first monday of the parsed activate date year as a base::Time
  // object. This will make it easier to add the required number of days to
  // get the start of the ISO 8601 week standard period.
  base::Time new_year_ts;
  std::string new_year_date =
      "Jan 01 00:00:00 GMT " + base::NumberToString(activate_year);
  bool success = base::Time::FromUTCString(new_year_date.c_str(), &new_year_ts);

  if (!success) {
    LOG(ERROR) << "Failed to store new year in base::Time using "
                  "FromUTCString method.";
    return absl::nullopt;
  }

  // ISO 8601 assigns the weeks to 0 if the stored date was
  // before the first monday of the year.
  // For example, the first monday of 2020 is Jan 6th, so devices that had
  // Activated between [Jan 1st, Jan 5th] have activate week of year set to 0.
  if (activate_week_of_year == 0) {
    return new_year_ts;
  }

  base::Time first_monday_of_year = GetFirstMondayFromNewYear(new_year_ts);

  // Get the number of days to the start of a ISO 8601 week standard period
  // for that year from the years first monday. This is equal to
  // (activate_week_of_year-1) * 7 days.
  int days_in_iso_period = 0;
  days_in_iso_period = (activate_week_of_year - 1) * 7;

  // Add the above two steps to get the start of a ISO 8601 week time.
  return first_monday_of_year + base::Days(days_in_iso_period);
}

absl::optional<base::Time> ActiveStatus::GetFirstActiveWeek() const {
  absl::optional<base::StringPiece> first_active_week_val =
      statistics_provider_->GetMachineStatistic(system::kActivateDateKey);
  std::string first_active_week_str =
      std::string(first_active_week_val.value_or(kActivateDateKeyNotFound));

  if (first_active_week_str == kActivateDateKeyNotFound) {
    LOG(ERROR)
        << "Failed to retrieve ActivateDate VPD field from machine statistics. "
        << "Leaving |first_active_week_| unset.";
    return absl::nullopt;
  }

  // Activate date is formatted: "YYYY-WW"
  int delimiter_index = first_active_week_str.find('-');

  const int expected_first_active_week_size = 7;
  const int expected_delimiter_index = 4;
  if (first_active_week_str.size() != expected_first_active_week_size ||
      delimiter_index != expected_delimiter_index) {
    LOG(ERROR) << "ActivateDate was retrieved but is not formatted as YYYY-WW. "
               << "Received string : " << first_active_week_str;
    return absl::nullopt;
  }

  const int expected_year_size = 4;
  const int expected_weeks_size = 2;

  std::string parsed_year = first_active_week_str.substr(0, expected_year_size);
  std::string parsed_weeks = first_active_week_str.substr(
      expected_delimiter_index + 1, expected_weeks_size);

  if (parsed_year.empty() || parsed_weeks.empty()) {
    LOG(ERROR) << "Failed to parse and convert the first active weeks string "
               << "year and weeks.";
    return absl::nullopt;
  }

  // Convert parsed year and weeks to int.
  int activate_year, activate_week_of_year;
  bool success_year = base::StringToInt(parsed_year, &activate_year);
  bool success_week = base::StringToInt(parsed_weeks, &activate_week_of_year);

  if (!success_year || !success_week) {
    LOG(ERROR) << "Failed to convert parsed_year or parsed_weeks: "
               << parsed_year << " and " << parsed_weeks;
    return absl::nullopt;
  }

  auto iso8601_ts = Iso8601DateWeekAsTime(activate_year, activate_week_of_year);
  if (!iso8601_ts.has_value()) {
    LOG(ERROR) << "Failed to ISO8601 year and week of year as a timestamp.";
    return absl::nullopt;
  }

  return iso8601_ts.value();
}

int ActiveStatus::GetMonthsSinceInception() const {
  std::string month_from_inception =
      ConvertIntegerToBitset<kActiveStatusBitSize>(GetValue())
          .to_string()
          .substr(0, kMonthCountBitSize);

  return ConvertBitsetToInteger<kMonthCountBitSize>(
      std::bitset<kMonthCountBitSize>(month_from_inception));
}

int ActiveStatus::GetActiveMonthBits() const {
  std::string active_months =
      ConvertIntegerToBitset<kActiveStatusBitSize>(GetValue())
          .to_string()
          .substr(kMonthCountBitSize,
                  kActiveStatusBitSize - kMonthCountBitSize);

  return ConvertBitsetToInteger<kActiveMonthsBitSize>(
      std::bitset<kActiveMonthsBitSize>(active_months));
}

bool ActiveStatus::IsDeviceActiveInMonth(int month_idx) const {
  DCHECK(month_idx >= 0 && month_idx <= 17) << "Month must be in [0,17] range.";
  return ConvertIntegerToBitset<kActiveMonthsBitSize>(GetActiveMonthBits())
      .test(month_idx);
}

absl::optional<bool> ActiveStatus::IsFirstActiveInCohort(
    base::Time active_ts) const {
  auto first_active_week = GetFirstActiveWeek();
  if (!first_active_week.has_value()) {
    LOG(ERROR)
        << "First Active Week could not be retrieved correctly from VPD.";
    return absl::nullopt;
  }

  base::Time::Exploded exploded;
  first_active_week.value().UTCExplode(&exploded);
  int first_active_year = exploded.year;
  int first_active_month = exploded.month;

  active_ts.UTCExplode(&exploded);
  int cohort_year = exploded.year;
  int cohort_month = exploded.month;

  return first_active_year == cohort_year && first_active_month == cohort_month;
}

}  // namespace ash::report::device_metrics
