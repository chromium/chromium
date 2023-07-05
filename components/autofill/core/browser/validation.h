// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_

#include <string>

#include "base/strings/string_piece_forward.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

// Constants for the length of a CVC.
static const size_t GENERAL_CVC_LENGTH = 3;
static const size_t AMEX_CVC_LENGTH = 4;

enum class CvcType {
  kUnknown = 0,
  // Regular card verification code. For CVC length, default to regular case of
  // the network.
  kRegularCvc = 1,
  // Three digit card verification code used for unique American Express cases.
  kBackOfAmexCvc = 2,
  kMaxValue = kBackOfAmexCvc,
};

// Returns true if |year| and |month| describe a date later than |now|.
// |year| must have 4 digits.
bool IsValidCreditCardExpirationDate(int year,
                                     int month,
                                     const base::Time& now);

// Returns true if |year| describes a year later than or equal to |now|'s year.
// |year| must have 4 digits.
bool IsValidCreditCardExpirationYear(int year, const base::Time& now);

// Returns true if |text| looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(const std::u16string& text);

// Returns true if |number| has correct length according to card network.
bool HasCorrectLength(const std::u16string& number);

// Returns true if |number| passes the validation by Luhn formula.
bool PassesLuhnCheck(const std::u16string& number);

// Returns true if |code| looks like a valid credit card security code
// for the given credit card network.
bool IsValidCreditCardSecurityCode(const std::u16string& code,
                                   const base::StringPiece card_network,
                                   CvcType cvc_type = CvcType::kRegularCvc);

// Returns true if |text| looks like a valid e-mail address.
bool IsValidEmailAddress(const std::u16string& text);

// Returns true if |text| is a valid US state name or abbreviation.  It is case
// insensitive.  Valid for US states only.
bool IsValidState(const std::u16string& text);

// Returns whether the number contained in |text| is possible phone number,
// either in international format, or in the national format associated with
// |country_code|. Callers should cache the result as the parsing is expensive.
bool IsPossiblePhoneNumber(const std::u16string& text,
                           const std::string& country_code);

// Returns true if |text| looks like a valid zip code.
// Valid for US zip codes only.
bool IsValidZip(const std::u16string& text);

// Returns true if |text| looks like an SSN, with or without separators.
bool IsSSN(const std::u16string& text);

// Returns the expected CVC length based on the |card_network|.
size_t GetCvcLengthForCardNetwork(const base::StringPiece card_network,
                                  CvcType cvc_type = CvcType::kRegularCvc);

// Returns true if |value| appears to be a UPI Virtual Payment Address.
// https://upipayments.co.in/virtual-payment-address-vpa/
bool IsUPIVirtualPaymentAddress(const std::u16string& value);

// Returns true if |value| appears to be an International Bank Account Number
// (IBAN). See https://en.wikipedia.org/wiki/International_Bank_Account_Number
bool IsInternationalBankAccountNumber(const std::u16string& value);

// Return true if |value| is a 3 or 4 digit number.
bool IsPlausibleCreditCardCVCNumber(const std::u16string& value);

// Returns true if the value is a 4 digit year in this century.
bool IsPlausible4DigitExpirationYear(const std::u16string& value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
