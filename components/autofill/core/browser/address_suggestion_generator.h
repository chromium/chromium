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
  explicit AddressSuggestionGenerator(AutofillClient& autofill_client);
  ~AddressSuggestionGenerator();
  AddressSuggestionGenerator(const AddressSuggestionGenerator&) = delete;
  AddressSuggestionGenerator& operator=(const AddressSuggestionGenerator&) =
      delete;

  // Generates suggestions for a form containing the given `field_types`. It
  // considers all available profiles, deduplicates them based on the types and
  // returns one suggestion per remaining profile.
  // `field_types` are the relevant types for the current suggestions.
  std::vector<Suggestion> GetSuggestionsForProfiles(
      const FieldTypeSet& field_types,
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      SuggestionType suggestion_type,
      AutofillSuggestionTriggerSource trigger_source);

  // Generates a separator suggestion.
  // TODO(b/41484171): Remove.
  static Suggestion CreateSeparator();

  // Generates a footer suggestion "Manage addresses..." menu item which will
  // redirect to Chrome address settings page.
  static Suggestion CreateManageAddressesEntry();

  // Generate "Clear form" suggestion.
  // TODO(b/41484171): Remove.
  static Suggestion CreateClearFormSuggestion();

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
      AutofillSuggestionTriggerSource trigger_source) const;

  // Returns a list of profiles that will be displayed as suggestions to the
  // user, sorted by their relevance. This involves many steps from fetching the
  // profiles to matching with `field_contents`, and deduplicating based on
  // `field_types`, which are the relevant types for the current suggestion.
  // `options` defines what strategies to follow by the function in order to
  // filter the list or returned profiles.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  GetProfilesToSuggest(FieldType trigger_field_type,
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
      uint64_t trigger_field_max_length);

  // Dedupes the given profiles based on if one is a subset of the other for
  // suggestions represented by `field_types`. The function returns at most
  // `kMaxDeduplicatedProfilesForSuggestion` profiles. `field_types` stores all
  // of the FieldTypes relevant for the current suggestions, including that of
  // the field on which the user is currently focused.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  DeduplicatedProfilesForSuggestions(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          matched_profiles,
      FieldType trigger_field_type,
      const FieldTypeSet& field_types,
      const AutofillProfileComparator& comparator);

  // Matches based on prefix search, and limits number of profiles.
  // Returns the top matching profiles based on prefix search. At most
  // `kMaxPrefixMatchedProfilesForSuggestion` are returned.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  GetPrefixMatchedProfiles(const std::vector<const AutofillProfile*>& profiles,
                           FieldType trigger_field_type,
                           const std::u16string& raw_field_contents,
                           const std::u16string& field_contents_canon,
                           bool field_is_autofilled);

  // Removes profiles that haven't been used after `kDisusedDataModelTimeDelta`
  // from `profiles`. Note that the goal of this filtering strategy is only to
  // reduce visual noise for users that have many profiles, and therefore in
  // some cases, some disused profiles might be kept in the list, to avoid
  // filtering out all profiles, leading to no suggestions being shown. The
  // relative ordering of `profiles` is maintained.
  void RemoveDisusedSuggestions(
      std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>& profiles)
      const;

  // Creates nested/child suggestions for `suggestion` with the `profile`
  // information. Uses `trigger_field_type` to define what group filling
  // suggestion to add (name, address or phone). The existence of child
  // suggestions defines whether the autofill popup will have submenus.
  void AddAddressGranularFillingChildSuggestions(FieldType trigger_field_type,
                                                 const AutofillProfile& profile,
                                                 Suggestion& suggestion) const;

  // Returns non address suggestions which are displayed below address
  // suggestions in the Autofill popup. `is_autofilled` is used to conditionally
  // add suggestion for clearing all autofilled fields.
  std::vector<Suggestion> GetAddressFooterSuggestions(bool is_autofilled) const;

  const AddressDataManager& address_data() const {
    // The PDM outlives the ASG, hence this is safe.
    return autofill_client_->GetPersonalDataManager()->address_data_manager();
  }

  // autofill_client_ and the generator are both one per tab, and have the same
  // lifecycle.
  raw_ref<AutofillClient> autofill_client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_H_
