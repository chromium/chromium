// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

// Helper methods specific for granular filling related behaviour.
namespace autofill {

// Helper method that returns all address related fields for the purpose of
// group filling. Because group filling groups differ from actual
// Autofill groups for addresses (for group filling we consider company fields
// to be of address type), this method is effectively the union of
// FieldTypeGroup::kAddress and FieldTypeGroup::kCompany.
ServerFieldTypeSet GetAddressFieldsForGroupFilling();

// Returns true if `fields` matches one of granullar
// filling groups, i.e., FieldTypeGroup::kName fields, FieldTypeGroup::kPhone
// fields or `GetAddressFieldsForGroupFilling` (from the method above).
bool AreFieldsGranularFillingGroup(const ServerFieldTypeSet& fields);

// Returns a set of fields to be filled, given the last targeted fields and
// the current field type. For example, if the last targeted fields matches
// one of the group filling sets, we will return the set of fields that matches
// the trigering field group. This is done so that the user stays at the same
// granularity as the one previously chosen.
ServerFieldTypeSet GetTargetServerFieldsForTypeAndLastTargetedFields(
    const ServerFieldTypeSet& last_targeted_fields,
    const AutofillType& type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GRANULAR_FILLING_UTILS_H_
