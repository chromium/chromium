// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

class AddressDataManager;
class AutofillClient;
class FormFieldData;

// Generates `SuggestionType::kAddressEntryOnTyping` suggestions based on prefix
// matching on unclassified fields. The suggestions returned will contain
// profile data whose prefix matches what the user has typed. As for now, only
// use the top profile to generate suggestions.
// `field_contents` is the string contained in the triggering field.
std::vector<Suggestion> GetSuggestionsOnTypingForProfile(
    const AddressDataManager& adress_data_manager,
    const std::u16string& field_contents);

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
    const FieldTypeSet& field_types);

// Exposes `CreateSuggestionsFromProfiles` in tests.
std::vector<Suggestion> CreateSuggestionsFromProfilesForTest(
    std::vector<AutofillProfile> profiles,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    uint64_t trigger_field_max_length,
    const std::string& app_locale = "en-US",
    std::optional<std::string> plus_address_email_override = std::nullopt,
    const std::string& gaia_email = "");

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_
