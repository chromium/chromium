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

// Default value for devices that are missing the activate date.
const char kActivateDateKeyNotFound[] = "ACTIVATE_DATE_KEY_NOT_FOUND";

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

int ChurnActiveStatus::GetMonthsSinceInception() {
  // Get first 10 bits of |value_| which represent the total months since
  // inception.
  std::string month_from_inception = value_.to_string().substr(
      kMonthsSinceInceptionBitOffset,
      kActiveMonthsBitOffset - kMonthsSinceInceptionBitOffset);

  return static_cast<int>(
      std::bitset<kMonthsSinceInceptionSize>(month_from_inception).to_ulong());
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
  DCHECK(first_active_week_ != base::Time());
  return first_active_week_;
}

void ChurnActiveStatus::SetFirstActiveWeek() {
  // Retrieve ActivateDate vpd field from machine statistics object.
  // Default |first_active_week_| to kActivateDateKeyNotFound if retrieval
  // from machine statistics fails.
  const absl::optional<base::StringPiece> first_active_week_val =
      statistics_provider_->GetMachineStatistic(system::kActivateDateKey);
  std::string first_active_week_str =
      std::string(first_active_week_val.value_or(kActivateDateKeyNotFound));

  if (first_active_week_str == kActivateDateKeyNotFound) {
    LOG(ERROR)
        << "Failed to retrieve ActivateDate VPD field from machine statistics. "
        << "Leaving |first_active_week_| unset.";
    return;
  }

  // Activate date is formatted: "YYYY-DD"
  int delimiter_index = first_active_week_str.find('-');

  const int expected_first_active_week_size = 7;
  const int expected_delimiter_index = 4;
  if (first_active_week_str.size() != expected_first_active_week_size ||
      delimiter_index != expected_delimiter_index) {
    LOG(ERROR) << "ActivateDate was retrieved but is not formatted as YYYY-DD. "
               << "Received string : " << first_active_week_str;
    return;
  }

  const int expected_year_size = 4;
  const int expected_month_size = 2;

  // Exploded object will use 1st day of month as the default.
  base::Time::Exploded exploded;
  bool parsed_year = base::StringToInt(
      first_active_week_str.substr(0, expected_year_size), &exploded.year);
  bool parsed_month =
      base::StringToInt(first_active_week_str.substr(
                            expected_delimiter_index + 1, expected_month_size),
                        &exploded.month);
  exploded.day_of_month = 1;

  // Verify that the StringToInt methods were able to parse the
  // |first_active_week_| string successfully for the year and month.
  if (!parsed_year || !parsed_month) {
    LOG(ERROR) << "Failed to parse and convert the first active week string "
               << "year and month.";
    return;
  }

  // Store |first_active_week_| using ActivateDate VPD UTC field.
  bool success = base::Time::FromUTCExploded(exploded, &first_active_week_);
  if (!success) {
    LOG(ERROR) << "ActivateDate could not be converted to base::Time "
               << "from exploded object.";
    return;
  }
}

}  // namespace ash::device_activity
