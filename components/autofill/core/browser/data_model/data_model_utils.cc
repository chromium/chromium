// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/data_model_utils.h"

#include "base/compiler_specific.h"
#include "base/i18n/string_search.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"

namespace autofill::data_util {

std::u16string Expiration2DigitMonthAsString(int expiration_month) {
  if (expiration_month < 1 || expiration_month > 12)
    return std::u16string();

  std::u16string month = base::NumberToString16(expiration_month);
  if (expiration_month >= 10)
    return month;

  std::u16string zero = u"0";
  return zero.append(month);
}

std::u16string Expiration2DigitYearAsString(int expiration_year) {
  if (expiration_year == 0)
    return std::u16string();

  std::u16string year = base::NumberToString16(expiration_year % 100);
  if (expiration_year >= 10)
    return year;

  std::u16string zero = u"0";
  return zero.append(year);
}

std::u16string Expiration4DigitYearAsString(int expiration_year) {
  if (expiration_year == 0)
    return std::u16string();

  return base::NumberToString16(expiration_year);
}

bool ParseExpirationMonth(const std::u16string& text,
                          const std::string& app_locale,
                          int* expiration_month) {
  std::u16string trimmed;
  base::TrimWhitespace(text, base::TRIM_ALL, &trimmed);

  if (trimmed.empty())
    return false;

  int month = 0;
  // Try parsing the |trimmed| as a number (this doesn't require |app_locale|).
  if (base::StringToInt(trimmed, &month))
    return SetExpirationMonth(month, expiration_month);

  if (app_locale.empty())
    return false;

  // Otherwise, try parsing the |trimmed| as a named month, e.g. "January" or
  // "Jan" in the user's locale.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale(app_locale.c_str());
  icu::DateFormatSymbols date_format_symbols(locale, status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);
  // Full months (January, Janvier, etc.)
  const base::span<const icu::UnicodeString> months = [&] {
    int32_t num_months;
    const icu::UnicodeString* months =
        date_format_symbols.getMonths(num_months);
    // SAFETY: getMonths returns a pointer to a (c-style) array of length
    // num_months.
    return UNSAFE_BUFFERS(
        base::span(months, base::checked_cast<size_t>(num_months)));
  }();
  for (size_t i = 0; i < months.size(); ++i) {
    const std::u16string icu_month(
        base::i18n::UnicodeStringToString16(months[i]));
    // We look for the ICU-defined month in |trimmed|.
    if (base::i18n::StringSearchIgnoringCaseAndAccents(icu_month, trimmed,
                                                       nullptr, nullptr)) {
      month = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return SetExpirationMonth(month, expiration_month);
    }
  }
  // Abbreviated months (jan., janv., fév.) Some abbreviations have . at the end
  // (e.g., "janv." in French). The period is removed.
  const base::span<const icu::UnicodeString> short_months = [&] {
    int32_t num_months;
    const icu::UnicodeString* months =
        date_format_symbols.getShortMonths(num_months);
    // SAFETY: getShortMonths returns a pointer to a (c-style) array of length
    // num_months.
    return UNSAFE_BUFFERS(
        base::span(months, base::checked_cast<size_t>(num_months)));
  }();
  base::TrimString(trimmed, u".", &trimmed);
  for (size_t i = 0; i < short_months.size(); ++i) {
    std::u16string icu_month(
        base::i18n::UnicodeStringToString16(short_months[i]));
    base::TrimString(icu_month, u".", &icu_month);
    // We look for the ICU-defined month in |trimmed_month|.
    if (base::i18n::StringSearchIgnoringCaseAndAccents(icu_month, trimmed,
                                                       nullptr, nullptr)) {
      month = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return SetExpirationMonth(month, expiration_month);
    }
  }

  return false;
}

bool ParseExpirationYear(const std::u16string& text, int* expiration_year) {
  std::u16string trimmed;
  base::TrimWhitespace(text, base::TRIM_ALL, &trimmed);

  int year = 0;
  if (!trimmed.empty() && !base::StringToInt(trimmed, &year))
    return false;

  return SetExpirationYear(year, expiration_year);
}

bool SetExpirationMonth(int value, int* expiration_month) {
  if (value < 0 || value > 12)
    return false;

  *expiration_month = value;
  return true;
}

bool SetExpirationYear(int value, int* expiration_year) {
  // If |value| is beyond this millennium, or more than 2 digits but
  // before the current millennium (e.g. "545", "1995"), return. What is left
  // are values like "45" or "2018".
  if (value > 2999 || (value > 99 && value < 2000))
    return false;

  // Will normalize 2-digit years to the 4-digit version.
  if (value > 0 && value < 100) {
    base::Time::Exploded now_exploded;
    AutofillClock::Now().LocalExplode(&now_exploded);
    value += (now_exploded.year / 100) * 100;
  }
  *expiration_year = value;
  return true;
}

std::u16string FindPossiblePhoneCountryCode(std::u16string_view text) {
  if (text.find(u"00") != std::u16string::npos ||
      text.find('+') != std::u16string::npos) {
    std::vector<std::u16string> captures;
    if (MatchesRegex<kAugmentedPhoneCountryCodeRe>(text, &captures)) {
      return captures[1];
    }
  }

  return std::u16string();
}

}  // namespace autofill::data_util
