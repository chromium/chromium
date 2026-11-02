// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/card_value_validation.h"

#include <algorithm>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_quality/validation.h"

namespace autofill {

namespace {

// Maximum length of a plausible cardholder name. Also stays well below
// `MatchesRegex()`'s 5000-character limit, above which `IsValidNameOnCard()`
// returns true even for strings with digits.
constexpr size_t kMaxNameLength = 100;

// The number of years after the current year that an expiration year may be.
constexpr int kMaxYearsInFuture = 20;

bool IsPlausibleMonth(std::u16string_view value) {
  // Check `size()` so that 3- or 4-digit CVCs with leading zeros (e.g. "007")
  // don't pass, and check `IsAsciiDigit` to reject "+".
  int month = 0;
  return value.size() <= 2 &&
         std::ranges::all_of(value, base::IsAsciiDigit<char16_t>) &&
         base::StringToInt(value, &month) && 1 <= month && month <= 12;
}

// Returns whether `value` is a 2-digit year or, if `allow_4_digits`, a 4-digit
// year within the next `kMaxYearsInFuture` years.
bool IsPlausibleYear(std::u16string_view value, bool allow_4_digits) {
  int year = 0;
  if (!std::ranges::all_of(value, base::IsAsciiDigit<char16_t>) ||
      !base::StringToInt(value, &year)) {
    return false;
  }
  if (value.size() != 2 && (value.size() != 4 || !allow_4_digits)) {
    return false;
  }
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  const int current_year = exploded.year;
  if (value.size() == 2) {
    // The first year with these last two digits that isn't in the past.
    year += current_year / 100 * 100;
    if (year < current_year) {
      year += 100;
    }
  }
  return current_year <= year && year <= current_year + kMaxYearsInFuture;
}

bool IsPlausibleExpirationDate(std::u16string_view value) {
  // Requiring a separator keeps 4-digit CVCs from passing as MMYY.
  const size_t separator = value.find_first_of(u"/-");
  if (separator == std::u16string_view::npos) {
    return false;
  }
  return IsPlausibleMonth(base::TrimWhitespace(value.substr(0, separator),
                                               base::TRIM_ALL)) &&
         IsPlausibleYear(
             base::TrimWhitespace(value.substr(separator + 1), base::TRIM_ALL),
             /*allow_4_digits=*/true);
}

}  // namespace

bool IsPlausibleNonSensitiveCreditCardValue(FieldType type,
                                            std::u16string_view value,
                                            bool is_select_element) {
  value = base::TrimWhitespace(value, base::TRIM_ALL);
  if (value.empty()) {
    return false;
  }
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
      return value.size() <= kMaxNameLength && IsValidNameOnCard(value);
    case CREDIT_CARD_EXP_MONTH:
      return IsPlausibleMonth(value);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return IsPlausibleYear(value, /*allow_4_digits=*/is_select_element);
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return IsPlausibleExpirationDate(value);
    default:
      return false;
  }
}

}  // namespace autofill
