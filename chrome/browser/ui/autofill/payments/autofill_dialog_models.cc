// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"

#include "base/strings/string16.h"
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
std::vector<base::string16> GetExpirationYearItems(int additional_year) {
  std::vector<base::string16> years;
  // Add the "Year" placeholder item.
  years.push_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR));

  base::Time::Exploded now_exploded;
  AutofillClock::Now().LocalExplode(&now_exploded);

  if (additional_year != 0 && additional_year < now_exploded.year)
    years.push_back(base::UTF8ToUTF16(std::to_string(additional_year)));

  for (int i = 0; i < kNumberOfExpirationYears; i++)
    years.push_back(base::UTF8ToUTF16(std::to_string(now_exploded.year + i)));

  if (additional_year != 0 &&
      additional_year >= now_exploded.year + kNumberOfExpirationYears) {
    years.push_back(base::UTF8ToUTF16(std::to_string(additional_year)));
  }

  return years;
}

// Formats a month, zero-padded (e.g. "02").
base::string16 FormatMonth(int month) {
  return base::ASCIIToUTF16(base::StringPrintf("%.2d", month));
}

}  // namespace

// MonthComboboxModel ----------------------------------------------------------

MonthComboboxModel::MonthComboboxModel() {}

MonthComboboxModel::~MonthComboboxModel() {}

int MonthComboboxModel::GetItemCount() const {
  // 12 months plus the empty entry.
  return 13;
}

base::string16 MonthComboboxModel::GetItemAt(int index) {
  return index == 0 ? l10n_util::GetStringUTF16(
                          IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH)
                    : FormatMonth(index);
}

void MonthComboboxModel::SetDefaultIndexByMonth(int month) {
  if (month >= 1 && month <= 12)
    default_index_ = month;
}

int MonthComboboxModel::GetDefaultIndex() const {
  return default_index_;
}

// YearComboboxModel -----------------------------------------------------------

YearComboboxModel::YearComboboxModel(int additional_year)
    : ui::SimpleComboboxModel(GetExpirationYearItems(additional_year)) {}

YearComboboxModel::~YearComboboxModel() {}

void YearComboboxModel::SetDefaultIndexByYear(int year) {
  const base::string16& year_value = base::NumberToString16(year);
  for (int i = 1; i < GetItemCount(); i++) {
    if (year_value == GetItemAt(i)) {
      default_index_ = i;
      return;
    }
  }
}

int YearComboboxModel::GetDefaultIndex() const {
  return default_index_;
}

}  // namespace autofill
