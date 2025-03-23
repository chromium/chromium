// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/data_model_utils.h"

#include <ranges>

#include "base/compiler_specific.h"
#include "base/i18n/string_search.h"
#include "base/i18n/unicodestring.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/dense_set.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"

namespace autofill::data_util {

namespace {

// Advances `input` if it starts with `token`.
bool Consume(std::u16string_view& input, std::u16string_view token) {
  if (input.starts_with(token)) {
    input = input.substr(token.size());
    return true;
  }
  return false;
}

// Consumes the next sequence of at least `min_num_digits` and at most
// `max_num_digits` digits from `input` and returns this sequence as `int`.
// Returns -1 if there is no such sequence. Advances `input` accordingly.
int ConsumeNumber(std::u16string_view& input,
                  size_t min_num_digits,
                  size_t max_num_digits,
                  bool allow_zero_padding) {
  size_t offset = 0;
  while (offset < max_num_digits && offset < input.size() &&
         base::IsAsciiDigit(input[offset])) {
    ++offset;
  }
  int num = 0;
  if ((!allow_zero_padding && input.starts_with(u'0') && offset > 1) ||
      offset < min_num_digits ||
      !base::StringToInt(input.substr(0, offset), &num)) {
    return -1;
  }
  input = input.substr(offset);
  return num;
}

// If `first_separator` is null, consumes an arbitrary separator and sets
// `first_separator` to that value.
// If `first_separator` is non-null, consumes it.
bool ConsumeSeparator(std::u16string_view& input,
                      const char16_t*& first_separator) {
  if (!first_separator) {
    for (const char16_t* separator :
         {u" / ", u" . ", u" - ", u"/", u".", u"-", u" ", u""}) {
      if (Consume(input, separator)) {
        first_separator = separator;
        return true;
      }
    }
    NOTREACHED();  // Because `kSeparators` contains u"".
  } else {
    return Consume(input, first_separator);
  }
}

}  // namespace

bool IsValidDateFormat(std::u16string_view format) {
  int year_width = 0;
  int month_width = 0;
  int day_width = 0;
  const char16_t* first_separator = nullptr;

  // Consumes a year, month, or day in the format `subformat`, if no year,
  // month, or day, respectively, has been already in an earlier call.
  auto consume_part = [&format](std::u16string_view subformat,
                                int& width) mutable {
    if (Consume(format, subformat) && width == 0) {
      width = subformat.size();
      return true;
    }
    return false;
  };

  // Consumes a year, month, or day, if no year, month, or day, respectively,
  // has been consumed in an earlier call.
  auto consume_any_part = [&]() {
    return consume_part(u"YYYY", year_width) ||
           consume_part(u"YY", year_width) ||
           consume_part(u"MM", month_width) ||
           consume_part(u"M", month_width) || consume_part(u"DD", day_width) ||
           consume_part(u"D", day_width);
  };

  // Consumes a separator. Subsequent calls only accept the separator that was
  // matched in the first call.
  auto consume_separator = [&format, &first_separator]() mutable {
    return ConsumeSeparator(format, first_separator);
  };

  return
      // At least one and at most three parts of distinct categories must be
      // present (e.g., YYYY-MM-MM are YYYY-MM/DD are not valid).
      consume_any_part() &&
      (format.empty() || (consume_separator() && consume_any_part())) &&
      (format.empty() || (consume_separator() && consume_any_part())) &&
      format.empty() &&
      // If both are present, month and day must agree on the width (e.g.,
      // YYYY-MM-D and YYYY-M-DD are not valid).
      (month_width == 0 || day_width == 0 || month_width == day_width) &&
      // If month or day are not alone and they're not long, there must be a
      // non-empty separator (e.g., DM and YYYYM are not valid).
      (month_width == 2 || day_width == 2 || !first_separator ||
       first_separator[0] != '\0');
}

bool ParseDate(std::u16string_view date,
               std::u16string_view format,
               Date& result,
               const char16_t*& first_separator) {
  first_separator = nullptr;

  // Consumes `part` (= YYYY, YY, MM, M, DD, or D) from `format` and the
  // corresponding numeric value from `date`. Returns that numeric value if
  // successful, and -1 otherwise.
  auto consume_part = [&date, &format](std::u16string_view part) {
    if (Consume(format, part)) {
      size_t min_num_digits = part.size();
      size_t max_num_digits = part.size() == 1 ? 2 : min_num_digits;
      bool allow_zero_padding = min_num_digits != 1;
      return ConsumeNumber(date, min_num_digits, max_num_digits,
                           allow_zero_padding);
    }
    return -1;
  };

  while (!date.empty() && !format.empty()) {
    int num = -1;
    if ((num = consume_part(u"YYYY")) >= 0) {
      result.year = num;
    } else if ((num = consume_part(u"YY")) >= 0) {
      result.year = 2000 + num;
    } else if ((num = consume_part(u"MM")) >= 0) {
      result.month = num;
    } else if ((num = consume_part(u"M")) >= 0) {
      result.month = num;
    } else if ((num = consume_part(u"DD")) >= 0) {
      result.day = num;
    } else if ((num = consume_part(u"D")) >= 0) {
      result.day = num;
    } else if (Consume(format, u"*")) {
      if (!ConsumeSeparator(date, first_separator)) {
        return false;
      }
    } else if (Consume(format, u"+")) {
      if (!ConsumeSeparator(date, first_separator) || !first_separator[0]) {
        return false;
      }
    } else if (!date.empty() && !format.empty() && date[0] == format[0]) {
      date = date.substr(1);
      format = format.substr(1);
    } else {
      return false;
    }
  }
  return date.empty() && format.empty();
}

bool IsValidDateForFormat(const Date& date, std::u16string_view format) {
  auto max_days = [](int year, int month) {
    if (month < 1 || month > 12) {
      return 31;
    }
    static constexpr std::array<int, 12> kDaysOfMonth{31, 28, 31, 30, 31, 30,
                                                      31, 31, 30, 31, 30, 31};
    bool has_leap_day =
        month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return kDaysOfMonth[month - 1] + (has_leap_day ? 1 : 0);
  };

  bool valid = true;
  while (!format.empty() && valid) {
    if (Consume(format, u"YYYY") || Consume(format, u"YY")) {
      valid = 0 < date.year && date.year <= 9999;
    } else if (Consume(format, u"MM") || Consume(format, u"M")) {
      valid = 0 < date.month && date.month <= 12;
    } else if (Consume(format, u"DD") || Consume(format, u"D")) {
      valid = 0 < date.day && date.day <= max_days(date.year, date.month);
    } else {
      format = format.substr(1);
    }
  }
  return valid;
}

std::u16string FormatDate(Date date, std::u16string_view format) {
  std::u16string result;
  result.reserve(format.size());
  auto append = [&](const std::u16string& number, size_t min_width) {
    size_t num = number.size();
    while (++num <= min_width) {
      result += u'0';
    }
    result += number;
  };

  while (!format.empty()) {
    if (Consume(format, u"YYYY")) {
      append(base::NumberToString16(date.year), 4);
    } else if (Consume(format, u"YY")) {
      append(base::NumberToString16(date.year % 100), 2);
    } else if (Consume(format, u"MM")) {
      append(base::NumberToString16(date.month), 2);
    } else if (Consume(format, u"M")) {
      append(base::NumberToString16(date.month), 0);
    } else if (Consume(format, u"DD")) {
      append(base::NumberToString16(date.day), 2);
    } else if (Consume(format, u"D")) {
      append(base::NumberToString16(date.day), 0);
    } else {
      result += format[0];
      format = format.substr(1);
    }
  }
  return result;
}

std::u16string Expiration2DigitMonthAsString(int expiration_month) {
  if (expiration_month < 1 || expiration_month > 12)
    return std::u16string();
  return FormatDate({.month = expiration_month}, u"MM");
}

std::u16string Expiration2DigitYearAsString(int expiration_year) {
  if (expiration_year == 0)
    return std::u16string();
  return FormatDate({.year = expiration_year}, u"YY");
}

std::u16string Expiration4DigitYearAsString(int expiration_year) {
  if (expiration_year == 0)
    return std::u16string();
  return FormatDate({.year = expiration_year}, u"YYYY");
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
