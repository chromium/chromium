// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillProfile;
class CreditCard;
class FormStructure;
class LoyaltyCard;

// For each submitted field in the `form_structure`, determines whether
// `ADDRESS_HOME_STATE` is a possible matching type.
// This function is intended to run on the UI thread and its result can be
// passed to DeterminePossibleFieldTypesForUpload().
[[nodiscard]] std::set<FieldGlobalId> PreProcessStateMatchingTypes(
    const std::vector<AutofillProfile>& profiles,
    const FormStructure& form_structure,
    const std::string& app_locale);

// Uses the existing personal data in `profiles`, `credit_cards` and
// `loyalty_cardss` to determine possible field types for the `form`.  This is
// potentially expensive -- on the order of 50ms even for a small set of
// `stored_data`. Hence, it should not run on the UI thread -- to avoid
// locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
void DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::vector<LoyaltyCard>& loyalty_cards,
    const std::set<FieldGlobalId>& fields_that_match_state,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure& form);

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
