// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/plus_addresses/plus_address_types.h"

namespace autofill {
class FormFieldData;
struct Suggestion;
}  // namespace autofill

namespace plus_addresses {

class PlusAddressSettingService;

// Helper class for generation plus address suggestions. Objects of this class
// are not intended to be saved into a member - instead, their lifetime should
// be scoped to a method call that generates suggestions.
class PlusAddressSuggestionGenerator final {
  STACK_ALLOCATED();

 public:
  PlusAddressSuggestionGenerator(
      const PlusAddressSettingService* setting_service,
      bool is_off_the_record);
  ~PlusAddressSuggestionGenerator();

  // Returns the suggestions to be offered on the `focused_field` with Password
  // Manager classification `focused_form_classification`. `affiliated_profiles`
  // are assumed to be the plus profiles affiliated with the primary main frame
  // origin.
  std::vector<autofill::Suggestion> GetSuggestions(
      const autofill::AutofillClient::PasswordFormClassification&
          focused_form_classification,
      const autofill::FormFieldData& focused_field,
      autofill::AutofillSuggestionTriggerSource trigger_source,
      std::vector<PlusProfile> affiliated_profiles) const;

  // Returns a suggestion for managing plus addresses.
  static autofill::Suggestion GetManagePlusAddressSuggestion();

 private:
  // Returns a suggestion to create a new plus address.
  autofill::Suggestion CreateNewPlusAddressSuggestion() const;

  const raw_ref<const PlusAddressSettingService> setting_service_;
  const bool is_off_the_record_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
