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

// Removes Unicode whitespace, dash punctuation, format
// characters (e.g. zero-width space, bidi marks), and dots, and converts
// Unicode decimal digits to ASCII digits. Used so that sensitive-value
// detectors (credit card, IBAN, SSN) cannot be evaded by such characters.
std::u16string StripSeparatorsAndNormalizeDigits(std::u16string_view value);

// Returns the internal representation of card issuer network corresponding to
// the given `number`.  The card issuer network is determined purely according
// to the Issuer Identification Number (IIN), a.k.a. the "Bank Identification
// Number (BIN)", which is parsed from the relevant prefix of the `number`. This
// function performs no additional validation checks on the `number`. Hence, the
// returned issuer network for both the valid card "4111-1111-1111-1111" and the
// invalid card "4garbage" will be Visa, which has an IIN of 4.
const char* GetCardNetwork(std::u16string_view number);

// The well-formatted full digits for display. A whitespace will be added as a
// separator between digits, e.g. "1234 5678 9000 0000"
std::u16string GetFormattedCardNumberForDisplay(std::u16string_view number);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_CREDIT_CARD_NUMBER_VALIDATION_H_
