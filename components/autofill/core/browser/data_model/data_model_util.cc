// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/data_model_util.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/string_search.h"
#include "base/i18n/unicodestring.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
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

bool IsWordChar(char16_t c) {
  return u_isalnum(c) || c == u'_';
}

bool MatchesMonth(std::u16string icu_month, std::u16string_view input) {
  if (icu_month.empty()) {
    return false;
  }
  size_t match_index = 0;
  size_t match_length = 0;
  if (!base::i18n::StringSearchIgnoringCaseAndAccents(
          std::move(icu_month), input, &match_index, &match_length)) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          features::kAutofillParseMonthFromStringByWordBoundary)) {
    return true;
  }
  // Verify that the `icu_month` is not a substring of a longer word.
  const bool has_leading_boundary =
      match_index == 0 || !IsWordChar(input[match_index - 1]);
  const size_t match_end = match_index + match_length;
  const bool has_trailing_boundary =
      match_end == input.size() || !IsWordChar(input[match_end]);
  return has_leading_boundary && has_trailing_boundary;
}

// Returns a span of localized symbols/names for months.
base::span<const icu::UnicodeString> GetMonthsSpan(
    const icu::DateFormatSymbols& date_format_symbols,
    icu::DateFormatSymbols::DtContextType context,
    icu::DateFormatSymbols::DtWidthType width) {
  int32_t num_months = 0;
  const icu::UnicodeString* months =
      date_format_symbols.getMonths(num_months, context, width);
  if (!months || num_months <= 0) {
    return {};
  }
  // SAFETY: `icu::DateFormatSymbols::getMonths()` sets `num_months` to the
  // length of the internal C-style array returned in `months`.
  return UNSAFE_BUFFERS(
      base::span(months, base::checked_cast<size_t>(num_months)));
}

// Returns whether `text` contains only Unicode digits and whitespace (or is
// empty).
bool ContainsOnlyDigitsOrWhitespace(std::u16string_view text) {
  for (base::i18n::UTF16CharIterator it(text); !it.end(); it.Advance()) {
    if (!u_isdigit(it.get()) && !u_isUWhiteSpace(it.get())) {
      return false;
    }
  }
  return true;
}

// Returns whether `code_point` is part of a word: an alphanumeric character,
// an underscore, or a Unicode combining mark (e.g., an NFD accent separating a
// base letter from a substring match, as in "de\u0301marche").
bool IsWordCodePoint(int32_t code_point) {
  const auto char_type = u_charType(code_point);
  return u_isalnum(code_point) || code_point == u'_' ||
         char_type == U_NON_SPACING_MARK ||
         char_type == U_COMBINING_SPACING_MARK || char_type == U_ENCLOSING_MARK;
}

// Returns whether a digit-leading `icu_month` (e.g. Japanese "1月".."12月") has
// a valid leading boundary given `prev_it` pointing at the code point
// immediately before the match.
// A non-digit predecessor (e.g. "年" in "2025年11月") is valid. A preceding
// digit is rejected unless it is a single leading zero before a 1-digit month
// (so zero-padded "01月" matches ICU's unpadded "1月", whereas "001月",
// "010月", and "13月" are rejected).
bool HasValidDigitLeadingBoundary(
    std::u16string_view icu_month,
    const base::i18n::UTF16CharIterator& prev_it) {
  const int32_t prev = prev_it.get();
  if (!u_isdigit(prev)) {
    return true;
  }
  // Two-digit month names (e.g. "10月".."12月") must never be preceded by
  // another digit (rejects e.g. "010月", "110月").
  base::i18n::UTF16CharIterator month_it(icu_month);
  if (u_isdigit(month_it.NextCodePoint())) {
    return false;
  }
  // Only zero-padding ('0' / fullwidth '０') preserves the month value. Other
  // leading digits (e.g. '1' in "11月" or "13月") change the numeric value
  // ("11月" is matched as month 11 earlier; "13月" is invalid).
  if (u_charDigitValue(prev) != 0) {
    return false;
  }
  // Reject multiple leading digits/zeros (e.g. "001月", "101月").
  return prev_it.start() || !u_isdigit(prev_it.PreviousCodePoint());
}

// Returns whether a match of non-empty `icu_month` at `target[start..end)` sits
// on valid leading and trailing boundaries:
// - Leading boundary (when `start > 0`):
//   - If `icu_month` starts with a digit (e.g. Japanese "1月".."12月"), the
//     preceding code point must not be a digit, except for a single leading
//     zero before a 1-digit month (so zero-padded Japanese "01月" matches ICU's
//     unpadded "1月" for January). Non-digit predecessors (e.g. "年" in
//     "2025年11月") are allowed.
//   - Otherwise, the preceding code point must not be a word code point
//     (rejects mid-word matches like "sie" in "Wrzesień" or "一月" in
//     "十一月").
// - Trailing boundary (when `end < target.size()`):
//   - The following code point must not be a word code point (rejects
//     continuations like "MayTheForceBeWithYou", "May2026", or "Tháng 1" in
//     "Tháng 10").
bool HasValidMonthBoundary(std::u16string_view icu_month,
                           std::u16string_view target,
                           size_t start,
                           size_t end) {
  if (start > 0) {
    // Step by UTF-16 code points so supplementary characters like "𐐀May" stay
    // intact.
    base::i18n::UTF16CharIterator prev_it =
        base::i18n::UTF16CharIterator::UpperBound(target, start);
    prev_it.Rewind();
    base::i18n::UTF16CharIterator month_start(icu_month);
    if (u_isdigit(month_start.get())) {
      if (!HasValidDigitLeadingBoundary(icu_month, prev_it)) {
        return false;
      }
    } else if (IsWordCodePoint(prev_it.get())) {
      return false;
    }
  }
  if (end < target.size()) {
    const int32_t next =
        base::i18n::UTF16CharIterator::UpperBound(target, end).get();
    if (IsWordCodePoint(next)) {
      return false;
    }
  }
  return true;
}

// Returns the length (in UTF-16 code units) of the first match of `icu_month`
// in `target` (after trimming leading and trailing periods, e.g. French
// "févr.") using case- and accent-insensitive substring search that satisfies
// `HasValidMonthBoundary()`, or `std::nullopt` if none matches. Returns
// `std::nullopt` if `icu_month` consists only of digits and whitespace (bare
// numeric months are handled before named-month lookup) or if `target` is
// empty.
std::optional<size_t> MatchMonthNameLength(std::u16string_view icu_month,
                                           std::u16string_view target) {
  icu_month = base::TrimString(icu_month, u".", base::TRIM_ALL);
  if (ContainsOnlyDigitsOrWhitespace(icu_month) || target.empty()) {
    return std::nullopt;
  }

  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher{
      std::u16string(icu_month)};
  size_t offset = 0;
  size_t match_index = 0;
  size_t match_length = 0;
  while (offset < target.size() &&
         searcher.Search(target.substr(offset), &match_index, &match_length)) {
    const size_t start = offset + match_index;
    const size_t end = start + match_length;
    if (match_length == 0 || end > target.size()) {
      return std::nullopt;
    }
    if (HasValidMonthBoundary(icu_month, target, start, end)) {
      return match_length;
    }
    base::i18n::UTF16CharIterator next_it =
        base::i18n::UTF16CharIterator::LowerBound(target, start);
    next_it.Advance();
    offset = next_it.array_pos();
  }
  return std::nullopt;
}

// Attempts to parse a localized month name from `trimmed` using ICU's
// `DateFormatSymbols` for `locale`, checking `WIDE` before `ABBREVIATED` across
// `STANDALONE` and `FORMAT` contexts.
std::optional<int> ParseNamedMonthForLocale(std::u16string_view trimmed,
                                            const icu::Locale& locale) {
  UErrorCode status = U_ZERO_ERROR;
  icu::DateFormatSymbols date_format_symbols(locale, status);
  if (U_FAILURE(status)) {
    return std::nullopt;
  }

  // Only `WIDE` and `ABBREVIATED` are checked: ICU aliases `SHORT` to
  // `ABBREVIATED` for months, and `NARROW` yields ambiguous single-character
  // initials (e.g. "J", "M") or bare digits.
  for (auto width :
       {icu::DateFormatSymbols::WIDE, icu::DateFormatSymbols::ABBREVIATED}) {
    std::optional<int> best_month;
    size_t best_match_length = 0;
    for (auto context :
         {icu::DateFormatSymbols::STANDALONE, icu::DateFormatSymbols::FORMAT}) {
      const base::span<const icu::UnicodeString> months =
          GetMonthsSpan(date_format_symbols, context, width);
      // Credit card expiration months use the 12-month Gregorian calendar.
      // Clamp to 12 because some non-Gregorian ICU calendars (e.g. Hebrew,
      // Ethiopic) return 13 entries and index 12 is never a valid month in
      // [1, 12].
      const int num_months =
          static_cast<int>(std::min<size_t>(months.size(), 12));
      // Test all 12 months and keep the longest match rather than assuming
      // longer names always appear later (e.g. Mongolian "арван нэгдүгээр сар"
      // [11] contains "нэгдүгээр сар" [1], whereas in CLDR Nuer "tiop thar pɛt"
      // [1] contains "pɛt" [2]).
      for (int i = 0; i < num_months; ++i) {
        if (std::optional<size_t> match_length = MatchMonthNameLength(
                base::i18n::UnicodeStringToString16(months[i]), trimmed);
            match_length && *match_length > best_match_length) {
          best_match_length = *match_length;
          best_month = i + 1;
        }
      }
    }
    if (best_month) {
      return best_month;
    }
  }
  return std::nullopt;
}

std::optional<int> ParseNamedMonth(std::u16string_view trimmed,
                                   std::string_view app_locale) {
  if (app_locale.empty()) {
    return ParseNamedMonthForLocale(trimmed, icu::Locale::getEnglish());
  }
  const icu::Locale locale(std::string(app_locale).c_str());
  if (locale.isBogus()) {
    return ParseNamedMonthForLocale(trimmed, icu::Locale::getEnglish());
  }
  return ParseNamedMonthForLocale(trimmed, locale)
      .or_else([&]() -> std::optional<int> {
        if (std::string_view(locale.getLanguage()) == "en") {
          return std::nullopt;
        }
        return ParseNamedMonthForLocale(trimmed, icu::Locale::getEnglish());
      });
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

bool IsValidAffixFormat(std::u16string_view format, bool exclude_full_value) {
  int i;
  return base::StringToInt(format, &i) &&
         ((i == 0 && !exclude_full_value) ||
          (std::abs(i) >=
               base::checked_cast<int>(kMinAffixLengthForFormatString) &&
           std::abs(i) <=
               base::checked_cast<int>(kMaxAffixLengthForFormatString)));
}

bool IsValidFlightNumberFormat(std::u16string_view format,
                               bool exclude_full_value) {
  return format == u"A" || format == u"N" ||
         (format == u"F" && !exclude_full_value);
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

std::optional<int> ParseMonthFromString(std::u16string_view text,
                                        std::string_view app_locale) {
  std::u16string_view trimmed = base::TrimWhitespace(text, base::TRIM_ALL);

  if (trimmed.empty())
    return std::nullopt;

  int month = 0;
  // Try parsing the `trimmed` as a number (this doesn't require `app_locale`).
  if (base::StringToInt(trimmed, &month))
    return GetExpirationMonth(month);

  if (base::FeatureList::IsEnabled(
          features::kAutofillFixParseMonthFromString)) {
    return ParseNamedMonth(trimmed, app_locale);
  }

  if (app_locale.empty())
    return std::nullopt;

  // Otherwise, try parsing the `trimmed` as a named month, e.g. "January" or
  // "Jan" in the user's locale.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale(std::string(app_locale).c_str());
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
    if (MatchesMonth(base::i18n::UnicodeStringToString16(months[i]), trimmed)) {
      month = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return GetExpirationMonth(month);
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
  trimmed = base::TrimString(trimmed, u".", base::TRIM_ALL);
  for (size_t i = 0; i < short_months.size(); ++i) {
    std::u16string icu_month(
        base::i18n::UnicodeStringToString16(short_months[i]));
    base::TrimString(icu_month, u".", &icu_month);
    if (MatchesMonth(std::move(icu_month), trimmed)) {
      month = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return GetExpirationMonth(month);
    }
  }

  return std::nullopt;
}

std::optional<int> ParseYearFromString(std::u16string_view text) {
  const std::u16string_view trimmed =
      base::TrimWhitespace(text, base::TRIM_ALL);

  int year = 0;
  if (!trimmed.empty() && !base::StringToInt(trimmed, &year))
    return std::nullopt;

  return GetExpirationYear(year);
}

bool ParseExpirationDate(std::u16string_view text,
                         std::u16string* month,
                         std::u16string* year) {
  static constexpr char16_t kDateRegex[] =
      uR"(^\s*[0-9]{1,2}\s*[-/|]?\s*[0-9]{2,4}\s*$)";
  if (!MatchesRegex<kDateRegex>(text)) {
    return false;
  }

  const size_t separator_pos = text.find_first_of(u"-/|");
  std::u16string_view month_result;
  std::u16string_view year_result;
  if (separator_pos != std::u16string_view::npos) {
    month_result =
        base::TrimWhitespace(text.substr(0, separator_pos), base::TRIM_ALL);
    year_result =
        base::TrimWhitespace(text.substr(separator_pos + 1), base::TRIM_ALL);
  } else if (text.size() % 2 == 0) {
    month_result = text.substr(0, 2);
    year_result = text.substr(2);
  } else {
    return false;
  }

  if (month) {
    *month = month_result;
  }
  if (year) {
    *year = year_result;
  }
  return true;
}

std::optional<int> GetExpirationMonth(int value) {
  if (value < 0 || value > 12)
    return std::nullopt;

  return value;
}

std::optional<int> GetExpirationYear(int value) {
  // If `value` is beyond this millennium, or more than 2 digits but
  // before the current millennium (e.g. "545", "1995"), return. What is left
  // are values like "45" or "2018".
  if (value > 2999 || (value > 99 && value < 2000))
    return std::nullopt;

  // Will normalize 2-digit years to the 4-digit version.
  if (value > 0 && value < 100) {
    base::Time::Exploded now_exploded;
    AutofillClock::Now().LocalExplode(&now_exploded);
    value += (now_exploded.year / 100) * 100;
  }
  return value;
}

std::u16string FindPossiblePhoneCountryCode(std::u16string_view text) {
  if (text.find(u"00") != std::u16string::npos ||
      text.find('+') != std::u16string::npos) {
    std::vector<std::u16string> captures;
    if (MatchesRegex<kAugmentedPhoneCountryCodeExtractionRe>(text, &captures)) {
      return captures[1];
    }
  }

  return std::u16string();
}

}  // namespace autofill::data_util
