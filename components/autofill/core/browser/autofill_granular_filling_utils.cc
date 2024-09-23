// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_granular_filling_utils.h"

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

FillingMethod GetFillingMethodFromTargetedFields(
    const FieldTypeSet& targeted_field_types) {
  if (targeted_field_types == kAllFieldTypes) {
    return FillingMethod::kFullForm;
  }
  if (targeted_field_types == GetFieldTypesOfGroup(FieldTypeGroup::kName)) {
    return FillingMethod::kGroupFillingName;
  }
  if (targeted_field_types == GetAddressFieldsForGroupFilling()) {
    return FillingMethod::kGroupFillingAddress;
  }
  if (targeted_field_types == GetFieldTypesOfGroup(FieldTypeGroup::kEmail)) {
    return FillingMethod::kGroupFillingEmail;
  }
  if (targeted_field_types == GetFieldTypesOfGroup(FieldTypeGroup::kPhone)) {
    return FillingMethod::kGroupFillingPhoneNumber;
  }
  if (targeted_field_types.size() == 1) {
    return FillingMethod::kFieldByFieldFilling;
  }
  return FillingMethod::kNone;
}

FillingMethod GetFillingMethodFromSuggestionType(SuggestionType type) {
  switch (type) {
    case SuggestionType::kFillFullAddress:
      return FillingMethod::kGroupFillingAddress;
    case SuggestionType::kFillFullName:
      return FillingMethod::kGroupFillingName;
    case SuggestionType::kFillFullPhoneNumber:
      return FillingMethod::kGroupFillingPhoneNumber;
    case SuggestionType::kFillFullEmail:
      return FillingMethod::kGroupFillingEmail;
    default:
      NOTREACHED();  // Unrelated SuggestionTypes.
  }
}

FieldTypeSet GetAddressFieldsForGroupFilling() {
  FieldTypeSet fields = GetFieldTypesOfGroup(FieldTypeGroup::kAddress);
  fields.insert_all(GetFieldTypesOfGroup(FieldTypeGroup::kCompany));
  return fields;
}

bool AreFieldsGranularFillingGroup(const FieldTypeSet& field_types) {
  return field_types == GetAddressFieldsForGroupFilling() ||
         field_types == GetFieldTypesOfGroup(FieldTypeGroup::kName) ||
         field_types == GetFieldTypesOfGroup(FieldTypeGroup::kEmail) ||
         field_types == GetFieldTypesOfGroup(FieldTypeGroup::kPhone);
}

FieldTypeSet GetTargetFieldTypesFromFillingMethod(
    FillingMethod filling_method) {
  switch (filling_method) {
    case FillingMethod::kFullForm:
      return kAllFieldTypes;
    case FillingMethod::kGroupFillingName:
      return GetFieldTypesOfGroup(FieldTypeGroup::kName);
    case FillingMethod::kGroupFillingAddress:
      return GetAddressFieldsForGroupFilling();
    case FillingMethod::kGroupFillingEmail:
      return GetFieldTypesOfGroup(FieldTypeGroup::kEmail);
    case FillingMethod::kGroupFillingPhoneNumber:
      return GetFieldTypesOfGroup(FieldTypeGroup::kPhone);
    case FillingMethod::kFieldByFieldFilling:
    case FillingMethod::kNone:
      NOTREACHED();
  }
}

}  // namespace autofill
