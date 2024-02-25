// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Number of years to be shown in the year combobox, including the current year.
// YearComboboxModel has the option of passing an additional year if not
// contained within the initial range.
const int kNumberOfExpirationYears = 10;

// Returns the items that are in the expiration year dropdown. If
// |additional_year| is not 0 and not within the normal range, it will be added
// accordingly.
std::vector<ui::SimpleComboboxModel::Item> GetExpirationYearItems(
    int additional_year) {
  std::vector<ui::SimpleComboboxModel::Item> years;
  // Add the "Year" placeholder item.
  years.emplace_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR));

  base::Time::Exploded now_exploded;
  AutofillClock::Now().LocalExplode(&now_exploded);

  if (additional_year != 0 && additional_year < now_exploded.year)
    years.emplace_back(base::NumberToString16(additional_year));

  for (int i = 0; i < kNumberOfExpirationYears; i++)
    years.emplace_back(base::NumberToString16(now_exploded.year + i));

  if (additional_year != 0 &&
      additional_year >= now_exploded.year + kNumberOfExpirationYears) {
    years.emplace_back(base::NumberToString16(additional_year));
  }

  return years;
}

// Formats a month, zero-padded (e.g. "02").
std::u16string FormatMonth(int month) {
  return base::ASCIIToUTF16(base::StringPrintf("%.2d", month));
}

}  // namespace

// MonthComboboxModel ----------------------------------------------------------

MonthComboboxModel::MonthComboboxModel() {}

MonthComboboxModel::~MonthComboboxModel() {}

size_t MonthComboboxModel::GetItemCount() const {
  // 12 months plus the empty entry.
  return 13;
}

std::u16string MonthComboboxModel::GetItemAt(size_t index) const {
  return index == 0 ? l10n_util::GetStringUTF16(
                          IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH)
                    : FormatMonth(static_cast<int>(index));
}

void MonthComboboxModel::SetDefaultIndexByMonth(int month) {
  if (month >= 1 && month <= 12)
    default_index_ = static_cast<size_t>(month);
}

std::optional<size_t> MonthComboboxModel::GetDefaultIndex() const {
  return default_index_;
}

// YearComboboxModel -----------------------------------------------------------

YearComboboxModel::YearComboboxModel(int additional_year)
    : ui::SimpleComboboxModel(GetExpirationYearItems(additional_year)) {}

YearComboboxModel::~YearComboboxModel() {}

void YearComboboxModel::SetDefaultIndexByYear(int year) {
  const std::u16string& year_value = base::NumberToString16(year);
  for (size_t i = 1; i < GetItemCount(); i++) {
    if (year_value == GetItemAt(i)) {
      default_index_ = i;
      return;
    }
  }
}

std::optional<size_t> YearComboboxModel::GetDefaultIndex() const {
  return default_index_;
}

}  // namespace autofill
