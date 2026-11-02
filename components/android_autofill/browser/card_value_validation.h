// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_CARD_VALUE_VALIDATION_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_CARD_VALUE_VALIDATION_H_

#include <string_view>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Returns whether `value`, which an Android Autofill service filled into a
// field that Autofill classified as `type`, looks like a value of `type`.
//
// Only the credit card types that the iframe security policy treats as
// non-sensitive are supported, i.e. the cardholder name and expiration date
// types, except CREDIT_CARD_TYPE. Returns false for all other types.
//
// The service, not Autofill, chooses the value, so the field may get any value,
// e.g. part of a card number, a CVC or a password. The checks are therefore
// strict:
// - Names must be at most 100 characters long and pass `IsValidNameOnCard()`
//   (no ASCII digits or certain special characters; capping the length also
//   avoids `MatchesRegex()`'s 5000-character limit).
// - Months must be 1 to 12 with 1 or 2 digits.
// - Years must have exactly 2 digits, or exactly 4 digits if
//   `is_select_element`, since CVCs can have 4 digits. They must be within the
//   next 20 years.
// - Expiration dates must be such a month and year separated by "/" or "-",
//   so that a 4-digit CVC can't pass as MMYY. Their years may have 4 digits in
//   any element.
bool IsPlausibleNonSensitiveCreditCardValue(FieldType type,
                                            std::u16string_view value,
                                            bool is_select_element);

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_CARD_VALUE_VALIDATION_H_
