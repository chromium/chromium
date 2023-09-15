
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_granular_filling_utils.h"

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

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
    const AutofillType& triggering_field_type) {
  if (AreFieldsGranularFillingGroup(last_targeted_fields)) {
    switch (triggering_field_type.group()) {
      case FieldTypeGroup::kName:
        return GetServerFieldTypesOfGroup(FieldTypeGroup::kName);
      case FieldTypeGroup::kAddress:
        return GetAddressFieldsForGroupFilling();
      case FieldTypeGroup::kPhone:
        return GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone);
      default:
        // If the 'current_granularity' is group filling, but the current
        // focused field is not one for which group we offer group filling
        // (name, address and phone field), we default back to fill full form
        // behaviour/pre-granular filling popup id.
        return kAllServerFieldTypes;
    }
  } else if (last_targeted_fields == kAllServerFieldTypes) {
    return kAllServerFieldTypes;
  } else if (last_targeted_fields.size() == 1) {
    return {triggering_field_type.GetStorableType()};
  } else {
    NOTREACHED_NORETURN();
  }
}

}  // namespace autofill
