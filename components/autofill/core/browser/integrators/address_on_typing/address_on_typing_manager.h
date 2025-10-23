// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// This class manages adding and removing field types to the Address on typing
// strike database.
class AddressOnTypingManager {
 public:
  explicit AddressOnTypingManager(
      AddressOnTypingSuggestionStrikeDatabase* strike_database);
  ~AddressOnTypingManager();

  AddressOnTypingManager(const AddressOnTypingManager&) = delete;
  AddressOnTypingManager& operator=(const AddressOnTypingManager&) = delete;

  // Called when a suggestion is shown, `field_types_used` is all the
  // `FieldType` used to build the Address on typing suggestions shown to the
  // user. Adds `field_types_used` to `unaccepted_field_types_`.
  void OnDidShowAddressOnTyping(FieldTypeSet field_types_used);

  // Called on suggestion acceptance, `field_type_used_to_build_suggestion` is
  // the `FieldType` used to build an Address on typing suggestion the user
  // just accepted. Removes `field_type_used_to_build_suggestion` from
  // `unaccepted_field_types_`.
  void OnDidAcceptAddressOnTyping(
      FieldType field_type_used_to_build_suggestion);

 private:
  raw_ptr<AddressOnTypingSuggestionStrikeDatabase> strike_database_;

  // For fields where `SuggestionType::kAddressEntryOnTyping`
  // suggestions were shown, stores the field type used to build each
  // suggestion. In order to later on know which suggestions types were not
  // accepted by the user, field types are removed from this variable on
  // `OnDidAcceptAddressOnTyping`.
  FieldTypeSet unaccepted_field_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_H_
