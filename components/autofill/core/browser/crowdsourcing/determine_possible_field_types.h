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
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace one_time_tokens {
class OneTimeToken;
}

namespace autofill {

class AutofillProfile;
class CreditCard;
class EntityInstance;
class AutofillField;
class LoyaltyCard;

// The result of DeterminePossibleFieldTypesForUpload() for a specific
// AutofillField.
struct PossibleTypes {
  PossibleTypes();
  PossibleTypes(const PossibleTypes&) = delete;
  PossibleTypes& operator=(const PossibleTypes&) = delete;
  PossibleTypes(PossibleTypes&&);
  PossibleTypes& operator=(PossibleTypes&&);
  ~PossibleTypes();

  // The FieldTypes for which data on file matches the field's value.
  FieldTypeSet types;

  // The format strings that match the field value.
  // Format strings are determined only for Autofill AI dates and identification
  // number (e.g., PASSPORT_NUMBER) affixes.
  std::set<std::pair<FormatString_Type, std::u16string>> formats;
};

// For each submitted field in the `form_structure`, determines whether
// `ADDRESS_HOME_STATE` is a possible matching type.
// This function is intended to run on the UI thread and its result can be
// passed to DeterminePossibleFieldTypesForUpload().
[[nodiscard]] std::set<FieldGlobalId> PreProcessStateMatchingTypes(
    base::span<const AutofillProfile*> profiles,
    base::span<const std::unique_ptr<AutofillField>> fields,
    const std::string& app_locale);

// Determines the `FieldType`s for which profiles etc. define non-empty
// values.
//
// This is potentially expensive -- on the order of 50ms even for a small set of
// `stored_data`. Hence, it should not run on the UI thread -- to avoid
// locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
[[nodiscard]] std::vector<PossibleTypes> DeterminePossibleFieldTypesForUpload(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    const std::set<FieldGlobalId>& fields_that_match_state,
    std::u16string_view last_unlocked_credit_card_cvc,
    base::span<const one_time_tokens::OneTimeToken> recent_otps,
    const std::string& app_locale,
    base::span<const std::unique_ptr<AutofillField>> fields);

// Returns the set of `FieldType`s for which the given profiles etc. contain
// non-empty values.
FieldTypeSet DetermineAvailableFieldTypes(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    std::u16string_view last_unlocked_credit_card_cvc,
    base::span<const one_time_tokens::OneTimeToken> recent_otps,
    const std::string& app_locale);

base::flat_set<std::pair<data_util::Date, PossibleTypes*>>
FindDatesAndSetFormatStringsForTesting(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<PossibleTypes> possible_types);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
