// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class AddressDataManager;
class AutofillClient;
class FormStructure;

// This class manages adding and removing field types to the Address on typing
// strike database.
// It is owned by `BrowserAutofillManager` and therefore its lifetime is bounded
// to the current frame.
class AddressOnTypingManager {
 public:
  explicit AddressOnTypingManager(AutofillClient& client);
  ~AddressOnTypingManager();

  AddressOnTypingManager(const AddressOnTypingManager&) = delete;
  AddressOnTypingManager& operator=(const AddressOnTypingManager&) = delete;

  // Used for logging and to handle the logic behind the strike database.
  // `field_global_id` is the id of the field where at least one
  // `SuggestionType::kAddressEntryOnTyping` suggestion was shown and
  // `autofill_field` is the `AutofillField` where the suggestion pop was shown.
  void OnDidShowAddressOnTyping(FieldGlobalId field_global_id,
                                AutofillField* autofill_field);

  // Used for logging and to handle the logic behind the strike database.
  // `field_global_id` is the id of the field where a
  // `SuggestionType::kAddressEntryOnTyping` was accepted. `value` is the the
  // literal string used to fill the field.
  // `field_type_used_to_build_suggestion` is the autofill `FieldType` from
  // which `value` was derived from.
  // `profile_used_guid` specifies the profile used to build
  // the accepted suggestion.
  void OnDidAcceptAddressOnTyping(FieldGlobalId field_global_id,
                                  const std::u16string& value,
                                  FieldType field_type_used_to_build_suggestion,
                                  const std::string profile_used_guid);

  void LogAddressOnTypingCorrectnessMetrics(const FormStructure& form);

 private:
  friend class AddressOnTypingManagerTestApi;

  AddressDataManager& address_data_manager() const;

  AddressOnTypingSuggestionStrikeDatabase* address_on_typing_strike_database();
  const AddressOnTypingSuggestionStrikeDatabase*
  address_on_typing_strike_database() const;

  // Adds a strike to block a field type when generating Address on typing
  // suggestions.
  void AddStrikeToBlockAddressOnTypingSuggestions(FieldType field_type);

  // Returns the max strike limit for the Address on typing strike database.
  // Returns `std::nullopt` if no strike database is defined.
  std::optional<int> GetAddressOnTypingMaxStrikesLimit() const;

  // Returns the current strikes count for the Address on typing strike database
  // for a certain `field_type`. Returns `std::nullopt` if no strike database is
  // defined.
  std::optional<int> GetAddressOnTypingFieldTypeStrikes(
      FieldType field_type) const;

  // Removes potential strikes to block an Address on typing suggestion to be
  // shown for a certain field type.
  void RemoveStrikesToBlockAddressOnTypingSuggestions(FieldType field_type);

  const raw_ref<AutofillClient> client_;

  // For fields where `SuggestionType::kAddressEntryOnTyping`
  // suggestions were shown, stores the field type used to build each
  // suggestion. In order to later on know which suggestions types were not
  // accepted by the user, field types are removed from this variable on
  // `OnDidAcceptAddressOnTyping`. Note that only the last time a suggestion is
  // shown is taken into account, meaning that if a user accepts a suggestion at
  // field A and latter does not accept at field B (for the same type), the type
  // used to build the suggestion is added to the strike database.
  FieldTypeSet unaccepted_field_types_;

  // For fields where `SuggestionType::kAddressEntryOnTyping`
  // suggestions were shown, stores whether the field is classified and the
  // `FieldTypeSet` used to build the suggestions, keyed by the field global
  // identifier.
  std::map<FieldGlobalId, std::pair<bool, FieldTypeSet>>
      fields_where_address_on_typing_was_shown_;

  // For profiles that were used to build
  // `SuggestionType::kAddressEntryOnTyping` suggestions, store their last usage
  // time keyed by the profile identifier.
  std::map<std::string, base::TimeDelta>
      address_on_typing_suggestion_profile_last_used_time_per_guid_;

  // Stores the identifiers of those profiles that were used to build
  // `SuggestionType::kAddressEntryOnTyping` suggestions and were later
  // accepted.
  std::set<std::string> address_on_typing_suggestion_accepted_profile_used_;

  // For fields where `SuggestionType::kAddressEntryOnTyping` suggestions were
  // accepted, stored the filled value. This is used later
  // for correctness metrics emission.
  std::map<FieldGlobalId, std::u16string> address_on_typing_value_used_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_H_
