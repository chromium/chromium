// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Return true if the `field` has at least one possible field type. A possible
// field type is every type that is neither UNKNOWN_TYPE or EMPTY_TYPE. The
// latter should never occur together with a possible field type.
// Should only be invoked after the possible types of `field` have been
// determined.
bool FieldHasMeaningfulPossibleFieldTypes(const AutofillField& field);

// Returns the number of possible field types (type votes) of a `field` that are
// in a specific `group`.
size_t NumberOfPossibleFieldTypesInGroup(const AutofillField& field,
                                         FieldTypeGroup group);

// Returns true if the type of `field` is a possible type.
bool TypeOfFieldIsPossibleType(const AutofillField& field);

// Returns true whenever type is ADDRESS_HOME_STREET_NAME or
// ADDRESS_HOME_HOUSE_NUMBER.
// TODO(crbug/1410438): Remove when cleaning up feature
// `kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete`.
bool IsStreetNameOrHouseNumberType(const ServerFieldType type);

// Returns true if `type` is address-related.
bool IsAddressType(const AutofillType& type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_
