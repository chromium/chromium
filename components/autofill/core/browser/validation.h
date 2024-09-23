// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_

#include <string>
#include <string_view>

namespace base {
class Time;
}  // namespace base

namespace autofill {

// Constants for the length of a CVC.
inline constexpr size_t GENERAL_CVC_LENGTH = 3;
inline constexpr size_t AMEX_CVC_LENGTH = 4;

enum class CvcType {
  kUnknown = 0,
  // Regular card verification code. For CVC length, default to regular case of
  // the network.
  kRegularCvc = 1,
  // Three digit card verification code used for unique American Express cases.
  kBackOfAmexCvc = 2,
  kMaxValue = kBackOfAmexCvc,
};

// Returns true if `year` and `month` describe a date later than `now`.
// `year` must have 4 digits.
bool IsValidCreditCardExpirationDate(int year, int month, base::Time now);

// Returns true if `year` describes a year later than or equal to `now`'s year.
// `year` must have 4 digits.
bool IsValidCreditCardExpirationYear(int year, base::Time now);

// Returns true if `code` looks like a valid credit card security code
// for the given credit card network.
bool IsValidCreditCardSecurityCode(std::u16string_view code,
                                   std::string_view card_network,
                                   CvcType cvc_type = CvcType::kRegularCvc);

// Returns true if `text` looks like a valid e-mail address.
bool IsValidEmailAddress(std::u16string_view text);

// Returns true if `text` is a valid US state name or abbreviation.  It is case
// insensitive.  Valid for US states only.
bool IsValidState(std::u16string_view text);

// Returns whether the number contained in `text` is possible phone number,
// either in international format, or in the national format associated with
// `country_code`. Callers should cache the result as the parsing is expensive.
bool IsPossiblePhoneNumber(const std::u16string& text,
                           const std::string& country_code);

// Returns true if `text` looks like a valid zip code.
// Valid for US zip codes only.
bool IsValidZip(std::u16string_view text);

// Returns true if `text` looks like an SSN, with or without separators.
bool IsSSN(std::u16string_view text);

// Returns the expected CVC length based on the `card_network`.
size_t GetCvcLengthForCardNetwork(std::string_view card_network,
                                  CvcType cvc_type = CvcType::kRegularCvc);

// Returns true if `value` appears to be a UPI Virtual Payment Address.
// https://upipayments.co.in/virtual-payment-address-vpa/
bool IsUPIVirtualPaymentAddress(std::u16string_view value);

// Returns true if `value` appears to be an International Bank Account Number
// (IBAN). See https://en.wikipedia.org/wiki/International_Bank_Account_Number
bool IsInternationalBankAccountNumber(std::u16string_view value);

// Return true if `value` is a 3 or 4 digit number.
bool IsPlausibleCreditCardCVCNumber(std::u16string_view value);

// Returns true if the value is a 4 digit year in this century.
bool IsPlausible4DigitExpirationYear(std::u16string_view value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
