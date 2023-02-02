// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class FormStructure;

enum class FormType : int {
  kUnknownFormType,
  kAddressForm,
  kCreditCardForm,
  kPasswordForm,
  kMaxValue = kPasswordForm
};

// Returns true if the form contains fields that represent the card number and
// the card expiration date.
bool FormHasAllCreditCardFields(const FormStructure& form_structure);

FormType FieldTypeGroupToFormType(FieldTypeGroup field_type_group);

base::StringPiece FormTypeToStringPiece(FormType form_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_TYPES_H_
