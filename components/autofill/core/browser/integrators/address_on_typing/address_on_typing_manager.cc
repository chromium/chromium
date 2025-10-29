// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"

namespace autofill {

AddressOnTypingManager::AddressOnTypingManager(
    AddressOnTypingSuggestionStrikeDatabase* strike_database)
    : strike_database_(strike_database) {}

AddressOnTypingManager::~AddressOnTypingManager() {
  if (!strike_database_) {
    return;
  }
  // If suggestions were shown but not accepted for a field, add a strike for
  // all the field types where a suggestion was shown.
  for (FieldType field_type_ignored : unaccepted_field_types_) {
      strike_database_->AddStrike(base::NumberToString(field_type_ignored));
      if (strike_database_->GetMaxStrikesLimit() ==
          strike_database_->GetStrikes(
              base::NumberToString(field_type_ignored))) {
        base::UmaHistogramSparse(
            "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
            field_type_ignored);
      }
  }
}

void AddressOnTypingManager::OnDidShowAddressOnTyping(
    FieldTypeSet field_types_used) {
  unaccepted_field_types_.insert_all(field_types_used);
}

void AddressOnTypingManager::OnDidAcceptAddressOnTyping(
    FieldType field_type_used_to_build_suggestion) {
  CHECK(unaccepted_field_types_.contains(field_type_used_to_build_suggestion));
  unaccepted_field_types_.erase(field_type_used_to_build_suggestion);

  // The user accepted a suggestion, so we should clear all strikes for this
  // field type.
  if (strike_database_) {
    strike_database_->ClearStrikes(
        base::NumberToString(field_type_used_to_build_suggestion));
  }
}

}  // namespace autofill
