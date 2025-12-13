// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_quality/validation.h"

#include <stddef.h>

#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/strings/grit/components_strings.h"

namespace autofill {

bool IsValidCreditCardExpirationDate(int year, int month, base::Time now) {
  if (month < 1 || month > 12) {
    return false;
  }

  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  // Convert 2-digit year to 4-digit year.
  if (year < 100) {
    year += (now_exploded.year / 100) * 100;
  }

  return year > now_exploded.year ||
         (year == now_exploded.year && month >= now_exploded.month);
}

bool IsValidCreditCardExpirationYear(int year, base::Time now) {
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  return year >= now_exploded.year;
}

bool IsValidCreditCardSecurityCode(std::u16string_view code,
                                   std::string_view card_network,
                                   CvcType cvc_type) {
  return code.length() == GetCvcLengthForCardNetwork(card_network, cvc_type) &&
         base::ContainsOnlyChars(code, u"0123456789");
}

bool IsValidEmailAddress(std::u16string_view text) {
  // E-Mail pattern as defined by the WhatWG. (4.10.7.1.5 E-Mail state)
  static constexpr char16_t kEmailPattern[] =
      u"^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\\.[a-zA-Z0-9-]+)*$";
  return MatchesRegex<kEmailPattern>(text);
}

bool IsValidState(std::u16string_view text) {
  return !state_names::GetAbbreviationForName(text).empty() ||
         !state_names::GetNameForAbbreviation(text).empty();
}

bool IsPossiblePhoneNumber(std::u16string_view text,
                           const std::string& country_code) {
  return i18n::IsPossiblePhoneNumber(base::UTF16ToUTF8(text), country_code);
}

bool IsValidZip(std::u16string_view text,
                const AddressCountryCode& country_code,
                bool extended_validation) {
  static constexpr char16_t kUsZipPattern[] = u"^\\d{5}(-\\d{4})?$";
  if (extended_validation) {
    // In most countries, a valid zip code string can contain only digits,
    // hyphens, and spaces.
    // [Ref: https://en.wikipedia.org/wiki/List_of_postal_codes]
    // \p{Nd} matches any Unicode decimal digit: standard digits (0-9),
    // full-width digits, Eastern Arabic numerals, etc.
    static constexpr char16_t kNumericZipPattern[] = u"^[\\p{Nd}- ]+$";
    static constexpr char16_t kJpZipCharacters[] = u"^[〒0-9- ０-９－　]+$";
    static constexpr char16_t kBrZipCharacters[] = u"^[0-9- .]+$";

    // Defines the lower boundary of zip code lengths for some countries with
    // split zip format. This check prevents a ZIP prefix (e.g., the first 3
    // digits out of 8 in JP) from being imported as a full ZIP code from a form
    // with split zip fields. The min length constant is simply the prefix
    // length + 1, because it's safer to use a smaller value than the exact
    // minimal zip length in case the zip format changes.
    // [Ref: https://en.wikipedia.org/wiki/List_of_postal_codes]
    static constexpr auto kZipCodeMinLengthMap =
        base::MakeFixedFlatMap<std::string_view, std::size_t>(
            {{"BR", 6}, {"CA", 4}, {"IE", 4}, {"JP", 4}, {"PL", 3}, {"PT", 5}});

    // A set of some of the biggest countries with a strictly numeric zip code
    // format.
    static constexpr auto kNumericZipCodeCountriesSet =
        base::MakeFixedFlatSet<std::string_view>(
            {"IN", "FR", "DE", "IT", "ES", "JP", "PL", "MX", "RU",
             "AU", "IR", "ZA", "BE", "ID", "PT", "PH", "RO", "TR",
             "SE", "CL", "CO", "CH", "AT", "SG", "CN", "SA"});
    auto it = kZipCodeMinLengthMap.find(country_code.value());
    if (it != kZipCodeMinLengthMap.end() && text.length() < it->second) {
      return false;
    }
    if (country_code == AddressCountryCode("US")) {
      return MatchesRegex<kUsZipPattern>(text);
    }
    if (country_code == AddressCountryCode("JP")) {
      return MatchesRegex<kJpZipCharacters>(text);
    }
    if (country_code == AddressCountryCode("BR")) {
      return MatchesRegex<kBrZipCharacters>(text);
    }
    if (base::Contains(kNumericZipCodeCountriesSet, country_code.value())) {
      return MatchesRegex<kNumericZipPattern>(text);
    }
    return true;
  } else {
    if (country_code != AddressCountryCode("US")) {
      return true;
    }
    return MatchesRegex<kUsZipPattern>(text);
  }
}

bool IsSSN(std::u16string_view text) {
  std::u16string number_string;
  base::RemoveChars(text, u"- ", &number_string);

  // A SSN is of the form AAA-GG-SSSS (A = area number, G = group number, S =
  // serial number). The validation we do here is simply checking if the area,
  // group, and serial numbers are valid.
  //
  // Historically, the area number was assigned per state, with the group number
  // ascending in an alternating even/odd sequence. With that scheme it was
  // possible to check for validity by referencing a table that had the highest
  // group number assigned for a given area number. (This was something that
  // Chromium never did though, because the "high group" values were constantly
  // changing.)
  //
  // However, starting on 25 June 2011 the SSA began issuing SSNs randomly from
  // all areas and groups. Group numbers and serial numbers of zero remain
  // invalid, and areas 000, 666, and 900-999 remain invalid.
  //
  // References for current practices:
  //   http://www.socialsecurity.gov/employer/randomization.html
  //   http://www.socialsecurity.gov/employer/randomizationfaqs.html
  //
  // References for historic practices:
  //   http://www.socialsecurity.gov/history/ssn/geocard.html
  //   http://www.socialsecurity.gov/employer/stateweb.htm
  //   http://www.socialsecurity.gov/employer/ssnvhighgroup.htm

  if (number_string.length() != 9 || !base::IsStringASCII(number_string)) {
    return false;
  }

  int area;
  if (!base::StringToInt(std::u16string_view(number_string).substr(0, 3),
                         &area)) {
    return false;
  }
  if (area < 1 || area == 666 || area >= 900) {
    return false;
  }

  int group;
  if (!base::StringToInt(std::u16string_view(number_string).substr(3, 2),
                         &group) ||
      group == 0) {
    return false;
  }

  int serial;
  if (!base::StringToInt(std::u16string_view(number_string).substr(5, 4),
                         &serial) ||
      serial == 0) {
    return false;
  }

  return true;
}

size_t GetCvcLengthForCardNetwork(std::string_view card_network,
                                  CvcType cvc_type) {
  if (card_network == kAmericanExpressCard &&
      cvc_type == CvcType::kRegularCvc) {
    return AMEX_CVC_LENGTH;
  }

  return GENERAL_CVC_LENGTH;
}

bool IsUPIVirtualPaymentAddress(std::u16string_view value) {
  return MatchesRegex<kUPIVirtualPaymentAddressRe>(value);
}

bool IsInternationalBankAccountNumber(std::u16string_view value) {
  std::u16string no_spaces;
  base::RemoveChars(value, u" ", &no_spaces);
  return MatchesRegex<kInternationalBankAccountNumberValueRe>(no_spaces);
}

bool IsAchRoutingTransitNumber(std::u16string_view value) {
  // For a 9-digit Routing Transit Number, multiply the first eight digits by
  // [3,7,1,3,7,1,3,7], and total the results. The remaining ninth digit is the
  // checksum necessary to make the total sum up to the nearest value that is
  // evenly divisible by 10.
  std::u16string trimmed_value;
  base::TrimWhitespace(value, base::TRIM_ALL, &trimmed_value);

  if (trimmed_value.length() != 9) {
    return false;
  }

  auto kMultipliers = std::to_array<int>({3, 7, 1, 3, 7, 1, 3, 7});
  int sum = 0;
  for (int i = 0; i < 8; ++i) {
    char16_t c = trimmed_value.at(i);
    if (!base::IsAsciiDigit(c)) {
      return false;
    }
    int digit = c - '0';
    sum += digit * kMultipliers[i];
  }

  // Do an extra `% 10` at the end to turn 10 into 0 if necessary.
  int expected_checksum_digit = (10 - (sum % 10)) % 10;

  char16_t checksum_char = trimmed_value.at(8);
  if (!base::IsAsciiDigit(checksum_char)) {
    return false;
  }
  int checksum_digit = checksum_char - '0';

  return checksum_digit == expected_checksum_digit;
}

bool IsPlausibleCreditCardCVCNumber(std::u16string_view value) {
  return MatchesRegex<kCreditCardCVCPattern>(value);
}

bool IsPlausible4DigitExpirationYear(std::u16string_view value) {
  return MatchesRegex<kCreditCard4DigitExpYearPattern>(value);
}

bool IsValidNameOnCard(std::u16string_view name) {
  static constexpr size_t kMaxNameOnCardLength = 26;
  static constexpr char16_t kInvalidNameCharacters[] =
      u"[0-9@#$^*()\\[\\]<>{}=?\"“”|•]";

  if (name.length() > kMaxNameOnCardLength) {
    return false;
  }

  return !MatchesRegex<kInvalidNameCharacters>(name);
}
}  // namespace autofill
