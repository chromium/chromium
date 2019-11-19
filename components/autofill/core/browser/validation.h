// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "components/autofill/core/browser/field_types.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

// Constants for the length of a CVC.
static const size_t GENERAL_CVC_LENGTH = 3;
static const size_t AMEX_CVC_LENGTH = 4;

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
bool IsValidCreditCardNumber(const base::string16& text);

// Returns true if |number| has correct length according to card network.
bool HasCorrectLength(const base::string16& number);

// Returns true if |number| passes the validation by Luhn formula.
bool PassesLuhnCheck(const base::string16& number);

// Returns true if |code| looks like a valid credit card security code
// for the given credit card type.
bool IsValidCreditCardSecurityCode(const base::string16& code,
                                   const base::StringPiece card_type);

// Returns true if |text| is a supported card type and a valid credit card
// number. |error_message| can't be null and will be filled with the appropriate
// error message.
bool IsValidCreditCardNumberForBasicCardNetworks(
    const base::string16& text,
    const std::set<std::string>& supported_basic_card_networks,
    base::string16* error_message);

// Returns true if |text| looks like a valid e-mail address.
bool IsValidEmailAddress(const base::string16& text);

// Returns true if |text| is a valid US state name or abbreviation.  It is case
// insensitive.  Valid for US states only.
bool IsValidState(const base::string16& text);

// Returns whether the number contained in |text| is possible phone number,
// either in international format, or in the national format associated with
// |country_code|. Callers should cache the result as the parsing is expensive.
bool IsPossiblePhoneNumber(const base::string16& text,
                           const std::string& country_code);

// Returns true if |text| looks like a valid zip code.
// Valid for US zip codes only.
bool IsValidZip(const base::string16& text);

// Returns true if |text| looks like an SSN, with or without separators.
bool IsSSN(const base::string16& text);

// Returns whether |value| is valid for the given |type|. If not null,
// |error_message| is populated when the function returns false.
bool IsValidForType(const base::string16& value,
                    ServerFieldType type,
                    base::string16* error_message);

// Returns the expected CVC length based on the |card_type|.
size_t GetCvcLengthForCardType(const base::StringPiece card_type);

// Returns true if |value| appears to be a UPI Virtual Payment Address.
// https://upipayments.co.in/virtual-payment-address-vpa/
bool IsUPIVirtualPaymentAddress(const base::string16& value);

// Returns true if |value| appears to be an International Bank Account Number
// (IBAN). See https://en.wikipedia.org/wiki/International_Bank_Account_Number
bool IsInternationalBankAccountNumber(const base::string16& value);

// Return true if |value| is a 3 or 4 digit number.
bool IsPlausibleCreditCardCVCNumber(const base::string16& value);

// Returns true if the value is a 4 digit year in this century.
bool IsPlausible4DigitExpirationYear(const base::string16& value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
