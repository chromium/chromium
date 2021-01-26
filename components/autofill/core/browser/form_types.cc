// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

FormType FieldTypeGroupToFormType(FieldTypeGroup field_type_group) {
  switch (field_type_group) {
    case NAME:
    case NAME_BILLING:
    case EMAIL:
    case COMPANY:
    case ADDRESS_HOME:
    case ADDRESS_BILLING:
    case PHONE_HOME:
    case PHONE_BILLING:
      return FormType::kAddressForm;
    case CREDIT_CARD:
      return FormType::kCreditCardForm;
    case USERNAME_FIELD:
    case PASSWORD_FIELD:
      return FormType::kPasswordForm;
    case NO_GROUP:
    case TRANSACTION:
    case UNFILLABLE:
      return FormType::kUnknownFormType;
  }
}
}  // namespace autofill
