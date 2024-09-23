// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/validation.h"

#include <stddef.h>

#include <ostream>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/strings/grit/components_strings.h"

namespace autofill {

bool IsValidCreditCardExpirationDate(int year, int month, base::Time now) {
  if (month < 1 || month > 12)
    return false;

  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
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

bool IsPossiblePhoneNumber(const std::u16string& text,
                           const std::string& country_code) {
  return i18n::IsPossiblePhoneNumber(base::UTF16ToUTF8(text), country_code);
}

bool IsValidZip(std::u16string_view text) {
  static constexpr char16_t kZipPattern[] = u"^\\d{5}(-\\d{4})?$";
  return MatchesRegex<kZipPattern>(text);
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

  if (number_string.length() != 9 || !base::IsStringASCII(number_string))
    return false;

  int area;
  if (!base::StringToInt(base::MakeStringPiece16(number_string.begin(),
                                                 number_string.begin() + 3),
                         &area)) {
    return false;
  }
  if (area < 1 || area == 666 || area >= 900) {
    return false;
  }

  int group;
  if (!base::StringToInt(base::MakeStringPiece16(number_string.begin() + 3,
                                                 number_string.begin() + 5),
                         &group) ||
      group == 0) {
    return false;
  }

  int serial;
  if (!base::StringToInt(base::MakeStringPiece16(number_string.begin() + 5,
                                                 number_string.begin() + 9),
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

bool IsPlausibleCreditCardCVCNumber(std::u16string_view value) {
  return MatchesRegex<kCreditCardCVCPattern>(value);
}

bool IsPlausible4DigitExpirationYear(std::u16string_view value) {
  return MatchesRegex<kCreditCard4DigitExpYearPattern>(value);
}
}  // namespace autofill
