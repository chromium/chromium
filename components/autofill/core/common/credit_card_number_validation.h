// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_CREDIT_CARD_NUMBER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_CREDIT_CARD_NUMBER_VALIDATION_H_

#include <string_view>

namespace autofill {

// Returns true if `text` looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(std::u16string_view text);

// Returns true if `number` has correct length according to card network.
bool HasCorrectCreditCardNumberLength(std::u16string_view number);

// Returns true if `number` passes the validation by Luhn formula.
bool PassesLuhnCheck(std::u16string_view number);

// Returns a version of `number` that has any separator characters removed.
std::u16string StripCardNumberSeparators(std::u16string_view number);

// Returns the internal representation of card issuer network corresponding to
// the given `number`.  The card issuer network is determined purely according
// to the Issuer Identification Number (IIN), a.k.a. the "Bank Identification
// Number (BIN)", which is parsed from the relevant prefix of the `number`. This
// function performs no additional validation checks on the `number`. Hence, the
// returned issuer network for both the valid card "4111-1111-1111-1111" and the
// invalid card "4garbage" will be Visa, which has an IIN of 4.
const char* GetCardNetwork(std::u16string_view number);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_CREDIT_CARD_NUMBER_VALIDATION_H_
