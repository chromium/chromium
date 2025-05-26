// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillProfile;
class CreditCard;
class EntityInstance;
class FormStructure;
class LoyaltyCard;

// For each submitted field in the `form_structure`, determines whether
// `ADDRESS_HOME_STATE` is a possible matching type.
// This function is intended to run on the UI thread and its result can be
// passed to DeterminePossibleFieldTypesForUpload().
[[nodiscard]] std::set<FieldGlobalId> PreProcessStateMatchingTypes(
    base::span<const AutofillProfile*> profiles,
    const FormStructure& form_structure,
    const std::string& app_locale);

// Determines the `FieldType`s for which profiles etc. define non-empty
// values. The result is stored in FormStructure::possible_types().
//
// This is potentially expensive -- on the order of 50ms even for a small set of
// `stored_data`. Hence, it should not run on the UI thread -- to avoid
// locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
void DeterminePossibleFieldTypesForUpload(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    const std::set<FieldGlobalId>& fields_that_match_state,
    std::u16string_view last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure& form);

// Returns the set of `FieldType`s for which the given profiles etc. contain
// non-empty values.
FieldTypeSet DetermineAvailableFieldTypes(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    std::u16string_view last_unlocked_credit_card_cvc,
    const std::string& app_locale);

// Matches the current field values against regular expressions.
// There are two types of matches:
// - An individual field with value "09/03/2025" matches "DD/MM/YYYY" and
//   "MM/DD/YYYY".
// - Three consecutive fields with values "09", "03", "2025" match "DD" and "D",
//   "MM" and "M", and "YYYY", respectively.
std::map<FieldGlobalId, base::flat_set<std::u16string>>
DeterminePossibleFormatStringsForUpload(
    base::span<const std::unique_ptr<AutofillField>> fields);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
