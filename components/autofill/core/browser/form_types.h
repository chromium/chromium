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
  kUnknownFormType,
  kAddressForm,
  kCreditCardForm,
  kPasswordForm,
  kStandaloneCvcForm,
  kMaxValue = kStandaloneCvcForm
};

// Enum for UMA metrics of the style
// Autofill.KeyMetrics.FillingAssistance.{FormTypeNameForLogging}
// TODO(crbug.com/339657029): add support for kEmailOnlyForm and
// kPostalAddressForm.
enum class FormTypeNameForLogging {
  kUnknownFormType,
  kAddressForm,
  kCreditCardForm,
  kPasswordForm,
  // Standalone CVC forms. These forms only contain one field and are solely
  // used for standalone CVC fields when cards are saved on file of a merchant
  // page.
  kStandaloneCvcForm,
  kMaxValue = kStandaloneCvcForm
};

std::string_view FormTypeNameForLoggingToStringView(
    FormTypeNameForLogging form_type);

// Returns true if the form contains fields that represent the card number and
// the card expiration date.
bool FormHasAllCreditCardFields(const FormStructure& form_structure);

FormType FieldTypeGroupToFormType(FieldTypeGroup field_type_group);

std::string_view FormTypeToStringView(FormType form_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_
