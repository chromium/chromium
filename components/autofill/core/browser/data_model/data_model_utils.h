// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

namespace autofill::data_util {

// A date consists of a year, month, and day.
// Zero values represent that the value is absent.
struct Date {
  friend bool operator==(const Date&, const Date&) = default;

  int year = 0;
  int month = 0;
  int day = 0;
};

// Returns true if `c` is one of the possible date format characters.
// See IsValidDateFormat() for details on separators.
constexpr bool IsDateSeparatorChar(char16_t c) {
  return c == '-' || c == '/' || c == '.' || c == ' ';
}

// Indicates if `format` is a date format string and nothing else.
//
// This is to ensure that `format` does not encode non-trivial information.
//
// A format string satisfies the following conditions:
// - It contains at least one placeholder for the year, the month, or the day.
// - It contains at most one placeholder for the year.
// - It contains at most one placeholder for the month.
// - It contains at most one placeholder for the day.
// - It contains a (possibly empty) separator between adjacent placeholders.
// - It must not contain two distinct separators.
// - If it contains month and day placeholders, they must be of equal length.
// - If it contains short month or day placeholders, any separator(s) must be
//   non-empty.
// - It must not contain anything else.
//
// The existing placeholders are:
// - YYYY (long) and YY (short) for year.
// - MM (long) and M (short) for month.
// - DD (long) and D (short) for day.
//
// The existing separators are /, ., -, optionally surrounded by one space on
// each side, or space itself or the empty string.
bool IsValidDateFormat(std::u16string_view format);

// Parses `date` according to `format` and populates the values in `result`
// accordingly. Returns `true` iff `date` fully matches the `format`.
//
// Wildcards:
// Typically, `format` satisfies IsValidDateFormat(). As an extension, `format`
// may also contain wildcards for separators: "*" matches any separator, "+"
// matches only non-empty separators. The first occurrence of a wildcard binds
// the wildcard, so that subsequent occurrences only match the identical
// separator.
// The out parameter `separator` is set to the separator if the wildcard matches
// any (including the empty separator). Otherwise, it is set to nullptr.
//
// Partial matches:
// For partial matches, the first matches are populated in `result`.
//
// Examples:
// - ParseDate(u"2025-09-10", u"YYYY-MM-DD", result, separator) returns true and
//   sets `result` to Date{.year = 2025, .month = 9, .day = 10} and `separator`
//   to nullptr.
// - ParseDate(u"2025-09-10", u"YYYY+MM+DD", result, separator) behaves as
//   above, except that it sets `separator` to u"-".
// - ParseDate(u"2025-09-10", u"YYYY*MM*DD", result, separator) behaves as
//   above, except that it sets `separator` to u"-".
// - ParseDate(u"20250910", u"YYYY*MM*DD", result, separator) behaves as
//   above, except that it sets `separator` to u"".
// - ParseDate(u"2025-XX-10", u"YYYY-MM-DD", result, separator) returns false
//   and sets `result.year` to 2025 and `separator` to nullptr.
// - ParseDate(u"20250910", u"YYYY+MM+DD", result, separator) returns false and
//   sets `result.year` to 2025 and `separator` to u"".
//
// This function is minimalistic and cheap (~1000x cheaper than parsing with
// ICU without caching the SimpleDateFormat).
bool ParseDate(std::u16string_view date,
               std::u16string_view format,
               Date& result,
               const char16_t*& wildcard_instance);

inline bool ParseDate(std::u16string_view date,
                      std::u16string_view format,
                      Date& result) {
  const char16_t* wildcard_instance = nullptr;
  return ParseDate(date, format, result, wildcard_instance);
}

// Returns true iff all values requested by `format` are valid in `date`:
// - If `format` contains a year, the date's year must be in the range 1 to 9999
//   (inclusive).
// - If `format` contains a month, the date's month must be in the range 1 to 12
//   (inclusive).
// - If `format` contains a day, the date's day must be in the range 1 to 28,
//   29, 30, or 31 (inclusive) depending on the month and year in the date. If
//   the date's month and/or year are zero, the function leans towards the
//   maximum (i.e., it assumes the year is a leap year and/or the month has 31
//   days).
bool IsValidDateForFormat(const Date& date, std::u16string_view format);

// Replaces the occurrences of YYYY, YY, MM, M, DD, D in `format` with the
// values from `date`.
//
// This function is minimalistic and cheap (~400x cheaper than parsing with
// ICU without caching the SimpleDateFormat).
std::u16string FormatDate(Date date, std::u16string_view format);

// Converts the integer |expiration_month| to std::u16string. Returns a value
// between ["01"-"12"].
std::u16string Expiration2DigitMonthAsString(int expiration_month);

// Converts the integer |expiration_year| to std::u16string. Returns a value
// between ["00"-"99"].
std::u16string Expiration2DigitYearAsString(int expiration_year);

// Converts the integer |expiration_year| to std::u16string.
std::u16string Expiration4DigitYearAsString(int expiration_year);

// Converts a string representation of a month (such as "February" or "feb."
// or "2") into a numeric value in [1, 12]. Returns true on successful
// conversion or false if a month was not recognized. When conversion fails,
// |expiration_month| is not modified.
bool ParseExpirationMonth(const std::u16string& text,
                          const std::string& app_locale,
                          int* expiration_month);

// Parses the |text| and stores the corresponding int value result in
// |*expiration_year|. This function accepts two digit years as well as
// four digit years between 2000 and 2999. Returns true on success.
// On failure, no change is made to |*expiration_year|.
bool ParseExpirationYear(const std::u16string& text, int* expiration_year);

// Sets |*expiration_month| to |value| if |value| is a valid month (1-12).
// Returns if any change is made to |*expiration_month|.
bool SetExpirationMonth(int value, int* expiration_month);

// Sets |*expiration_year| to |value| if |value| is a valid year. See comments
// in the function body for the definition of "valid". Returns if any change is
// made to |*expiration_year|.
bool SetExpirationYear(int value, int* expiration_year);

// Finds possible country code in |text| by fetching the first sub-group when
// matched with |kAugmentedPhoneCountryCodeRe| regex. It basically looks for a
// phone country code in the style of "+49" or "0049" in |text|. Preceding and
// following text is allowed unless that text contains digits. It returns the
// country code in the form of "49" in the example or an empty string.
std::u16string FindPossiblePhoneCountryCode(std::u16string_view text);

}  // namespace autofill::data_util

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_
