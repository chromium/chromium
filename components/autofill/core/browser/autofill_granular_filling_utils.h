// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

// Helper methods specific for granular filling related behavior.
namespace autofill {

// Autofill's possible filling methods. A filling method represents the
// different popup surfaces a user can use to interact with Autofill, which may
// lead to a different set of fields being filled. These sets/groups can be
// either the full form, a group of related fields or a single field.
enum class AutofillFillingMethod {
  // User chose to fill the whole form. Either from the main suggestion or from
  // the extended menu `PopupItemId::kFillEverything`.
  kFullForm = 0,
  // User chose one of the group filling options, such as name, address or phone
  // number.
  kGroupFilling = 1,
  // User chose to fill a specific field.
  kFieldByFieldFilling = 2,
  kNone = 3,
};

// Helper method that returns all address related fields for the purpose of
// group filling. Because group filling groups differ from actual
// Autofill groups for addresses (for group filling we consider company fields
// to be of address type), this method is effectively the union of
// FieldTypeGroup::kAddress and FieldTypeGroup::kCompany.
ServerFieldTypeSet GetAddressFieldsForGroupFilling();

// Returns true if `fields` matches one of granular filling groups, i.e.,
// FieldTypeGroup::kName, FieldTypeGroup::kPhone, FieldTypeGroup::kEmail  or
// `GetAddressFieldsForGroupFilling()`, see from the method above.
bool AreFieldsGranularFillingGroup(const ServerFieldTypeSet& field_types);

// Returns the autofill filling method corresponding to `targeted_fields`.
AutofillFillingMethod GetFillingMethodFromTargetedFields(
    const ServerFieldTypeSet& targeted_field_types);

// Returns a set of fields to be filled, given the last targeted fields and
// the current trigger field type. For example, if the last targeted fields
// matches one of the group filling sets, we will return the set of fields that
// matches the triggering field group. This is done so that the user stays at
// the same granularity as the one previously chosen.
ServerFieldTypeSet GetTargetServerFieldsForTypeAndLastTargetedFields(
    const ServerFieldTypeSet& last_targeted_field_types,
    ServerFieldType trigger_field_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GRANULAR_FILLING_UTILS_H_
