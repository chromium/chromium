// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_granular_filling_utils.h"

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

FieldTypeSet GetAddressFieldsForGroupFilling() {
  FieldTypeSet fields = GetFieldTypesOfGroup(FieldTypeGroup::kAddress);
  fields.insert_all(GetFieldTypesOfGroup(FieldTypeGroup::kCompany));
  return fields;
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
