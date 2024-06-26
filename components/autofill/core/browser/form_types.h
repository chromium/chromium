// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_

#include <string_view>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class FormStructure;

// Logged in UKM. Do not change or re-use values.
enum class FormType {
  kUnknownFormType = 0,
  kAddressForm = 1,
  kCreditCardForm = 2,
  kPasswordForm = 3,
  kStandaloneCvcForm = 4,
  kMaxValue = kStandaloneCvcForm
};

// Enum for UMA metrics of the style
// Autofill.KeyMetrics.FillingAssistance.{FormTypeNameForLogging}.
// These values are persisted to UKM logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FormTypeNameForLogging {
  kUnknownFormType = 0,
  kAddressForm = 1,
  kCreditCardForm = 2,
  kPasswordForm = 3,
  // Standalone CVC forms. These forms only contain one field and are solely
  // used for standalone CVC fields when cards are saved on file of a merchant
  // page.
  kStandaloneCvcForm = 4,
  // An email only form contains at least one email field. Other fields must be
  // unknown, password manager related or additional email fields. This
  // describes a subset of `kAddressForm` forms.
  kEmailOnlyForm = 5,
  // A postal address form contains at least 3 distinct field types of
  // `FieldTypeGroup::kAddress`, not counting a country field. Also store
  // locator forms with 3 fields (postal code, city, zip) are not included. This
  // describes a subset of `kAddressForm` forms.
  kPostalAddressForm = 6,
  kMaxValue = kPostalAddressForm
};

// The strings returned by this function are persisted to logs. Don't change the
// strings or existing mappings from `FormTypeNameForLogging` to string.
std::string_view FormTypeNameForLoggingToStringView(
    FormTypeNameForLogging form_type);

// Returns true if the form contains fields that represent the card number and
// the card expiration date.
bool FormHasAllCreditCardFields(const FormStructure& form_structure);

FormType FieldTypeGroupToFormType(FieldTypeGroup field_type_group);

std::string_view FormTypeToStringView(FormType form_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_
