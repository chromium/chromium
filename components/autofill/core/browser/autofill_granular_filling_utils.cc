
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

}  // namespace autofill
