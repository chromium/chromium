// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_granular_filling_utils.h"

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

ServerFieldTypeSet GetServerFieldsForFieldGroup(FieldTypeGroup group) {
  switch (group) {
    case FieldTypeGroup::kName:
      return GetServerFieldTypesOfGroup(FieldTypeGroup::kName);
    case FieldTypeGroup::kAddress:
      return GetAddressFieldsForGroupFilling();
    case FieldTypeGroup::kPhone:
      return GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone);
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kBirthdateField:
    case FieldTypeGroup::kIban:
      // If `group` is not one of the groups we offer group filling for
      // (name, address and phone field), we default back to fill full form
      // behaviour/pre-granular filling.
      return kAllServerFieldTypes;
  }
}

}  // namespace

AutofillFillingMethod GetFillingMethodFromTargetedFields(
    const ServerFieldTypeSet& targeted_fields) {
  if (targeted_fields == kAllServerFieldTypes) {
    return AutofillFillingMethod::kFullForm;
  }
  if (AreFieldsGranularFillingGroup(targeted_fields)) {
    return AutofillFillingMethod::kGroupFilling;
  }
  if (targeted_fields.size() == 1) {
    return AutofillFillingMethod::kFieldByFieldFilling;
  }
  return AutofillFillingMethod::kNone;
}

ServerFieldTypeSet GetAddressFieldsForGroupFilling() {
  ServerFieldTypeSet fields =
      GetServerFieldTypesOfGroup(FieldTypeGroup::kAddress);
  fields.insert_all(GetServerFieldTypesOfGroup(FieldTypeGroup::kCompany));
  return fields;
}

bool AreFieldsGranularFillingGroup(const ServerFieldTypeSet& fields) {
  return fields == GetAddressFieldsForGroupFilling() ||
         fields == GetServerFieldTypesOfGroup(FieldTypeGroup::kName) ||
         fields == GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone);
}

ServerFieldTypeSet GetTargetServerFieldsForTypeAndLastTargetedFields(
    const ServerFieldTypeSet& last_targeted_fields,
    ServerFieldType triggering_field_type) {
  switch (GetFillingMethodFromTargetedFields(last_targeted_fields)) {
    case AutofillFillingMethod::kGroupFilling:
      return GetServerFieldsForFieldGroup(
          GroupTypeOfServerFieldType(triggering_field_type));
    case AutofillFillingMethod::kFullForm:
      return kAllServerFieldTypes;
    case AutofillFillingMethod::kFieldByFieldFilling:
      return {triggering_field_type};
    case AutofillFillingMethod::kNone:
      break;
  }
  NOTREACHED_NORETURN();
}

}  // namespace autofill
