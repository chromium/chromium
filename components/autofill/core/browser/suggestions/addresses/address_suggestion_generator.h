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
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/suggestions/addresses/address_on_typing_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
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
// TODO(crbug.com/409962888): Remove once the new suggestion generation logic is
// launched.
std::vector<Suggestion> GetSuggestionsOnTypingForProfile(
    const AutofillClient& client,
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
    std::optional<std::string> plus_address_email_override = std::nullopt,
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
  // TODO(crbug.com/409962888): `plus_address_email_override`
  // has to be removed once the plus address suggestion generator and
  // suggestions merging are implemented.
  AddressSuggestionGenerator(
      const std::optional<std::string>& plus_address_email_override,
      LogManager* log_manager);
  ~AddressSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

 private:
  // Returns a vector of `AutofillProfile`s that will be suggested on a
  // `trigger_field` in a `form`. Can be empty if there is no data available for
  // filling or the filling conditions were not met.
  std::vector<AutofillProfile> MaybeFetchRegularAddressSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client);

  // Returns a vector of suggestions that will be suggested on a
  // `trigger_field` in a `form`.
  std::vector<Suggestion> GenerateAddressSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      std::vector<AutofillProfile>& profiles_to_suggest,
      const std::optional<std::string>& plus_address_email_override);

  // Used to change the emails matching the GAIA email in suggestions with
  // the `plus_address_email_override_`.
  // TODO(crbug.com/409962888): `plus_address_email_override_` has to be removed
  // once the plus address suggestion generator and suggestions merging are
  // implemented.
  const std::optional<std::string> plus_address_email_override_;

  raw_ptr<LogManager> log_manager_;

  // Stores a set of types of fillable fields that are in the form.
  FieldTypeSet field_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_SUGGESTION_GENERATOR_H_
