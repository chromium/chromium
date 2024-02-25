// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// All FieldTypes stored for an AutofillProfile in the local_addresses or
// contact_info table (depending on the profile source) in AutofillTable.
// When introducing a new field type, it suffices to add it here. When removing
// a field type, removing it from the list suffices (no additional clean-up in
// AutofillTable necessary). This is not reusing
// `AutofillProfile::SupportedTypes()` for three reasons:
// - The supported types are a function of the country. The types stored in the
//   table are country-independent and contain all the types relevant to any
//   country.
// - Due to the table design, the stored types are already ambiguous, so we
//   prefer the explicitness here.
// - Some supported types (like PHONE_HOME_CITY_CODE) are not stored.
// - Some non-supported types are stored (usually types that don't have
//   filling support yet).
const FieldTypeSet& GetDatabaseStoredTypesOfAutofillProfile();

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

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPE_UTILS_H_
