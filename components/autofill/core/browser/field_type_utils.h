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

// Returns true if `type` is address-related.
bool IsAddressType(FieldType type);

// Returns 0-based index of an address line type, which is X-1 for
// ADDRESS_HOME_LINEX. Expects only ADDRESS_HOME_LINE(1|2|3) types.
size_t AddressLineIndex(FieldType type);

// Returns whether the expiration year should be filled with two or four
// digits.
size_t DetermineExpirationYearLength(FieldType assumed_field_type);

// Returns true if `type` is alternative name related.
bool IsAlternativeNameType(FieldType type);

// A tag type is a type that doesn't provide complete information about a field
// on its own, and that instead needs a second type to complement its meaning.
bool IsTagType(FieldType type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_
