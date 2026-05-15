// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_

#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind_internal.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AddressDataManager;
class AutofillClient;
class FormFieldData;

// Generates `SuggestionType::kAddressEntryOnTyping` suggestions based on prefix
// matching on unclassified fields. The suggestions returned will contain
// profile data whose prefix matches what the user has typed. As for now, only
// use the top profile to generate suggestions.
// `field_contents` is the string contained in the triggering field.
// TODO(crbug.com/409962888): Remove once the new suggestion generation logic is
// launched.
std::vector<Suggestion> GetSuggestionsOnTypingForProfile(
    AutofillClient& client,
    const FormData& form,
    const FormFieldData& trigger_field);

// Generates a footer suggestion "Manage addresses..." menu item which will
// redirect to Chrome address settings page.
Suggestion CreateManageAddressesSuggestion();

// Exposes `GetProfilesToSuggest` in tests.
std::vector<AutofillProfile> GetProfilesToSuggestForTest(
    const AddressDataManager& address_data,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    const FieldTypeSet& field_types);

// Exposes `CreateSuggestionsFromProfiles` in tests.
std::vector<Suggestion> CreateSuggestionsFromProfilesForTest(
    std::vector<AutofillProfile> profiles,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    const FormFieldData& trigger_field,
    const std::string& app_locale = "en-US",
    const std::string& gaia_email = "");

// Returns true if a suggestion for an `AutofillProfile` with record type equal
// to `record_type` exists in the `suggestions`, false otherwise.
bool ContainsProfileSuggestionWithRecordType(
    base::span<const Suggestion> suggestions,
    const AddressDataManager& address_data_manager,
    AutofillProfile::RecordType record_type);

// `SuggestionGenerator` implementation for addresses. Obtaining the address
// suggestions should be done only through this class.
class AddressSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit AddressSuggestionGenerator(
      AutofillSuggestionTriggerSource trigger_source);
  ~AddressSuggestionGenerator() override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

 private:
  AutofillSuggestionTriggerSource trigger_source_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_
