// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

class AddressDataManager;
class AutofillClient;
class FormFieldData;

struct ProfilesToSuggestOptions {
  const bool exclude_disused_addresses = true;
  const bool require_non_empty_value_on_trigger_field = true;
  const bool prefix_match_suggestions = true;
  const bool deduplicate_suggestions = true;
};

// Generates suggestions for a form containing the given `field_types`. It
// considers all available profiles, deduplicates them based on the types and
// returns one suggestion per remaining profile.
// `field_types` are the relevant types for the current suggestions.
std::vector<Suggestion> GetSuggestionsForProfiles(
    const AutofillClient& client,
    const FieldTypeSet& field_types,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    SuggestionType suggestion_type,
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<std::string> plus_address_email_override);

// Generates a footer suggestion "Manage addresses..." menu item which will
// redirect to Chrome address settings page.
Suggestion CreateManageAddressesSuggestion();

// Exposes `GetProfilesToSuggest` in tests.
std::vector<AutofillProfile> GetProfilesToSuggestForTest(
    const AddressDataManager& address_data,
    FieldType trigger_field_type,
    const std::u16string& field_contents,
    bool field_is_autofilled,
    const FieldTypeSet& field_types,
    AutofillSuggestionTriggerSource trigger_source =
        AutofillSuggestionTriggerSource::kFormControlElementClicked);

// Exposes `CreateSuggestionsFromProfiles` in tests.
std::vector<Suggestion> CreateSuggestionsFromProfilesForTest(
    std::vector<AutofillProfile> profiles,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    uint64_t trigger_field_max_length,
    bool is_off_the_record = false,
    const std::string& app_locale = "en-US",
    std::optional<std::string> plus_address_email_override = std::nullopt,
    const std::string& gaia_email = "");

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_
