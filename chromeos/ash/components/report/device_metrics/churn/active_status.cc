// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/churn/active_status.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "components/prefs/pref_service.h"

namespace ash::report::device_metrics {

namespace {

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
    : local_state_(local_state) {}

int ActiveStatus::GetValue() const {
  return local_state_->GetInteger(
      ash::report::prefs::kDeviceActiveLastKnownChurnActiveStatus);
}

void ActiveStatus::SetValue(int val) {
  return local_state_->SetInteger(
      ash::report::prefs::kDeviceActiveLastKnownChurnActiveStatus, val);
}

std::optional<int> ActiveStatus::CalculateNewValue(base::Time ts) const {
  if (ts.is_null() || ts == base::Time::UnixEpoch()) {
    LOG(ERROR) << "Cannot calculate new value for invalid ts.";
    return std::nullopt;
  }

  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  int year = exploded.year;
  int month = exploded.month;

  // Calculate total number of months since January 2000 to current month.
  // e.g. Dec 2022 should return a total of 275 months.
  int new_months_from_inception =
      ((year - kInceptionYear) * utils::kMonthsInYear) + (month - 1);
  int previous_months_from_inception = GetMonthsSinceInception();

  // Check |ts| represents a new month than previously reported.
  if (new_months_from_inception <= previous_months_from_inception) {
    LOG(ERROR) << "Failed to update churn active status. "
               << "New number of months must be larger than the previous.";
    LOG(ERROR) << "Previous months from inception = "
               << previous_months_from_inception;
    LOG(ERROR) << "New months from inception = " << new_months_from_inception;

    return std::nullopt;
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

std::optional<base::Time> ActiveStatus::GetCurrentActiveMonthTimestamp() const {
  DCHECK_GE(GetMonthsSinceInception(), 0);

  int months_from_inception = GetMonthsSinceInception();
  std::optional<base::Time> inception_ts = GetInceptionMonthTimestamp();
  if (!inception_ts.has_value()) {
    LOG(ERROR) << "Failed to get the inception month as timestamp.";
    return std::nullopt;
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
    return std::nullopt;
  }

  return current_active_month_ts;
}

std::optional<ChurnCohortMetadata> ActiveStatus::CalculateCohortMetadata(
    base::Time active_ts) const {
  ChurnCohortMetadata metadata;

  std::optional<int> new_active_status = CalculateNewValue(active_ts);
  if (!new_active_status.has_value()) {
    LOG(ERROR) << "Failed to generate new value. Old Value = " << GetValue();
    return std::nullopt;
  }

  metadata.set_active_status_value(new_active_status.value());

  std::optional<bool> is_first_active = IsFirstActiveInCohort(active_ts);
  if (is_first_active.has_value()) {
    metadata.set_is_first_active_in_cohort(is_first_active.value());
  }

  return metadata;
}

std::optional<ChurnObservationMetadata>
ActiveStatus::CalculateObservationMetadata(base::Time active_ts,
                                           int period) const {
  DCHECK(period >= 0 && period <= 2) << "Period must be in [0,2] range.";

  // Observation metadata is generated if cohort ping was sent for the month.
  std::optional<base::Time> cur_active_month_ts =
      GetCurrentActiveMonthTimestamp();
  if (cur_active_month_ts.has_value() &&
      !utils::IsSameYearAndMonth(cur_active_month_ts.value(), active_ts)) {
    LOG(ERROR) << "Observation metadata require a current active status value. "
               << "This occurs after successful cohort pinging.";
    return std::nullopt;
  }

  ChurnObservationMetadata metadata;

  bool is_monthly_active = IsDeviceActiveInMonth(kMonthlyChurnOffset + period);
  bool is_yearly_active = IsDeviceActiveInMonth(kYearlyChurnOffset + period);
  metadata.set_monthly_active_status(is_monthly_active);
  metadata.set_yearly_active_status(is_yearly_active);

  std::optional<base::Time> first_active_week = utils::GetFirstActiveWeek();
  if (!first_active_week.has_value()) {
    LOG(ERROR) << "Cannot calculate observation metadata for first active "
               << "during cohort without the first active week.";
    return metadata;
  }

  std::optional<base::Time> last_month_ts = utils::GetPreviousMonth(active_ts);
  std::optional<base::Time> two_months_ago_ts =
      utils::GetPreviousMonth(last_month_ts.value_or(base::Time()));
  std::optional<base::Time> three_months_ago_ts =
      utils::GetPreviousMonth(two_months_ago_ts.value_or(base::Time()));

  if (!last_month_ts.has_value() || !two_months_ago_ts.has_value() ||
      !three_months_ago_ts.has_value()) {
    LOG(ERROR) << "Failed to calculate observation metadata for period.";
    return std::nullopt;
  }

  std::optional<base::Time> month_before_observation_period_start_ts;
  std::optional<base::Time> year_before_observation_period_start_ts;

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

std::optional<base::Time> ActiveStatus::GetInceptionMonthTimestamp() const {
  base::Time inception_ts;
  bool success = base::Time::FromUTCString(
      ActiveStatus::kActiveStatusInceptionDate, &inception_ts);

  if (!success) {
    LOG(ERROR) << "Failed to convert kActiveStatusInceptionDate to timestamp.";
    return std::nullopt;
  }

  return inception_ts;
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

std::optional<bool> ActiveStatus::IsFirstActiveInCohort(
    base::Time active_ts) const {
  auto first_active_week = utils::GetFirstActiveWeek();
  if (!first_active_week.has_value()) {
    LOG(ERROR)
        << "First Active Week could not be retrieved correctly from VPD.";
    return std::nullopt;
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
