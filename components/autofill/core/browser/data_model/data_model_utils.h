// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_

#include <string>
#include <string_view>

namespace autofill::data_util {

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
