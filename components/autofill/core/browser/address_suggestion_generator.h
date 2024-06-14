// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_

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

// Helper class to generate Autofill suggestions, such as for credit card and
// address profile Autofill.
class AddressSuggestionGenerator {
 public:
  AddressSuggestionGenerator();
  ~AddressSuggestionGenerator();
  AddressSuggestionGenerator(const AddressSuggestionGenerator&) = delete;
  AddressSuggestionGenerator& operator=(const AddressSuggestionGenerator&) =
      delete;

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
      AutofillSuggestionTriggerSource trigger_source);

  // Generates a footer suggestion "Manage addresses..." menu item which will
  // redirect to Chrome address settings page.
  static Suggestion CreateManageAddressesEntry();

 private:
  friend class AddressSuggestionGeneratorTestApi;

  struct ProfilesToSuggestOptions {
    const bool exclude_disused_addresses;
    const bool require_non_empty_value_on_trigger_field;
    const bool prefix_match_suggestions;
    const bool deduplicate_suggestions;
  };

  ProfilesToSuggestOptions GetProfilesToSuggestOptions(
      FieldType trigger_field_type,
      const std::u16string& trigger_field_contents,
      bool trigger_field_is_autofilled,
      AutofillSuggestionTriggerSource trigger_source);

  // Returns a list of profiles that will be displayed as suggestions to the
  // user, sorted by their relevance. This involves many steps from fetching the
  // profiles to matching with `field_contents`, and deduplicating based on
  // `field_types`, which are the relevant types for the current suggestion.
  // `options` defines what strategies to follow by the function in order to
  // filter the list or returned profiles.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  GetProfilesToSuggest(const AddressDataManager& address_data,
                       FieldType trigger_field_type,
                       const std::u16string& field_contents,
                       bool field_is_autofilled,
                       const FieldTypeSet& field_types,
                       ProfilesToSuggestOptions options);

  // Returns a list of Suggestion objects, each representing an element in
  // `profiles`.
  // `field_types` holds the type of fields relevant for the current suggestion.
  // The profiles passed to this function should already have been matched on
  // `trigger_field_contents_canon` and deduplicated.
  std::vector<Suggestion> CreateSuggestionsFromProfiles(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const FieldTypeSet& field_types,
      SuggestionType suggestion_type,
      FieldType trigger_field_type,
      uint64_t trigger_field_max_length,
      bool is_off_the_record,
      const std::string& app_locale);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_
