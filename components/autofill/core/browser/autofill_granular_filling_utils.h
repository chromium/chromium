// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"

// Helper methods specific for granular filling related behavior.
namespace autofill {

// Autofill's possible filling methods. A filling method represents the
// different popup surfaces a user can use to interact with Autofill, which may
// lead to a different set of fields being filled. These sets/groups can be
// either the full form, a group of related fields or a single field.
enum class FillingMethod : uint8_t {
  // User chose to fill the whole form. Either from the main suggestion or from
  // the extended menu `SuggestionType::kFillEverything`.
  kFullForm = 0,
  // User chose to fill all name fields.
  kGroupFillingName = 1,
  // User chose to fill all address fields.
  kGroupFillingAddress = 2,
  // User chose to fill all email fields.
  kGroupFillingEmail = 3,
  // User chose to fill all phone number fields.
  kGroupFillingPhoneNumber = 4,
  // User chose to fill a specific field.
  kFieldByFieldFilling = 5,
  // Used for default values.
  kNone = 6,
  kMaxValue = kNone
};

// Helper method that returns all address related fields for the purpose of
// group filling. Because group filling groups differ from actual
// Autofill groups for addresses (for group filling we consider company fields
// to be of address type), this method is effectively the union of
// FieldTypeGroup::kAddress and FieldTypeGroup::kCompany.
FieldTypeSet GetAddressFieldsForGroupFilling();

// Returns true if `fields` matches one of granular filling groups, i.e.,
// FieldTypeGroup::kName, FieldTypeGroup::kPhone, FieldTypeGroup::kEmail  or
// `GetAddressFieldsForGroupFilling()`, see from the method above.
bool AreFieldsGranularFillingGroup(const FieldTypeSet& field_types);

// Returns the autofill filling method corresponding to `targeted_fields`.
FillingMethod GetFillingMethodFromTargetedFields(
    const FieldTypeSet& targeted_field_types);

FillingMethod GetFillingMethodFromSuggestionType(SuggestionType type);

FieldTypeSet GetTargetFieldTypesFromFillingMethod(FillingMethod filling_method);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_GRANULAR_FILLING_UTILS_H_
