// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_CARD_VALUE_VALIDATION_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_CARD_VALUE_VALIDATION_H_

#include <string_view>

#include "base/containers/span.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/origin.h"

namespace autofill {

class FormFieldData;
class FormStructure;

// Outcome of validating a filled main-frame cardholder name or expiration date
// field in `GetPlausibleNonSensitiveCreditCardTypes()`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AndroidAutofillMainFrameCreditCardFieldValidationResult)
enum class MainFrameCreditCardFieldValidationResult {
  kAllowed = 0,
  kImplausibleValue = 1,
  kImplausibleSelectOptionText = 2,
  kDuplicateExpirationPart = 3,
  kMaxValue = kDuplicateExpirationPart,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AndroidAutofillMainFrameCreditCardFieldValidationResult)

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

// Returns the types that the iframe security policy should get, instead of
// UNKNOWN_TYPE, when a fill triggered from `triggered_origin` fills
// `filled_fields` of `form`.
//
// Returns an empty map if:
// - `triggered_origin` is `form`'s main-frame origin (since the security policy
//   already allows filling all main-frame fields in that case without
//   consulting field types),
// - `filled_fields` doesn't put a valid credit card number into a field of
//   `triggered_origin` that Autofill classified as a credit card number (the
//   classification alone isn't enough, since main-frame fields can affect how
//   fields of other frames are classified), or
// - `filled_fields` puts a value into a password control outside the main
//   frame's origin, unless it's a credit card number or verification code
//   field with a matching value.
//
// Otherwise, returns a type for those `filled_fields` that
// - are in `form`'s main-frame origin,
// - are not password controls (`FormControlType::kInputPassword`),
// - Autofill classified as a cardholder name or expiration date, and
// - got a value that passes `IsPlausibleNonSensitiveCreditCardValue()`. For
//   <select> elements, the texts of the options with that value must pass,
//   too, since the service may have picked the option by its text.
//
// At most one month, one year and one expiration date field get a type (the
// first in `filled_fields`), so that at most one short block of a card number
// that is split across several fields can pass the policy.
//
// When evaluating `filled_fields`, records
// `Autofill.WebView.CrossOriginMainFrameCreditCardFieldValidation` for each
// filled non-password main-frame field that Autofill classified as a cardholder
// name or expiration date.
absl::flat_hash_map<FieldGlobalId, FieldType>
GetPlausibleNonSensitiveCreditCardTypes(
    const FormStructure& form,
    base::span<const FormFieldData> filled_fields,
    const url::Origin& triggered_origin);

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_CARD_VALUE_VALIDATION_H_
