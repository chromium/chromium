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

// Indicates if `format` is (only) a date format string.
//
// A format string contains
// - at least a year, a month, or a day placeholder
// - at most one year, at most one month, and at most one day placeholder,
// - optional separators between these placeholders, and
// - nothing else.
//
// A year placeholder is YYYY or YY, a month placeholder is MM or M, a day
// placeholder is DD or D. Separators are /, ., -, optionally surrounded by
// one space on each side, or space itself. If `format` contains two separators,
// they must be identical.
//
// This is to ensure that `format` does not encode non-trivial information.
bool IsValidDateFormat(std::u16string_view format);

// Parses `date` according to `format` and populates the values in `result`
// accordingly. Returns `true` iff `date` matches the `format`.
//
// For example, ParseDate(u"2025-09-10", u"YYYY-MM-DD", result) returns true and
// sets `result` to Date{.year = 2025, .month = 9, .day = 10}.
//
// For partial matches, the first matches are populated in `result`. For
// example, ParseDate(u"2025-XX-10", u"YYYY-MM-DD", result) returns false but
// does populate `result.year`.
//
// This function is minimalistic and cheap (~1000x cheaper than parsing with
// ICU without caching the SimpleDateFormat).
bool ParseDate(std::u16string_view date,
               std::u16string_view format,
               Date& result);

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
