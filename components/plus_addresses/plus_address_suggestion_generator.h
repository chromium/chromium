// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/plus_addresses/plus_address_types.h"
#include "url/origin.h"

namespace autofill {
class FormFieldData;
struct Suggestion;
}  // namespace autofill

namespace plus_addresses {

class PlusAddressAllocator;
class PlusAddressSettingService;

// Helper class for generation plus address suggestions. Objects of this class
// are not intended to be saved into a member - instead, their lifetime should
// be scoped to a method call that generates suggestions.
class PlusAddressSuggestionGenerator final {
  STACK_ALLOCATED();

 public:
  PlusAddressSuggestionGenerator(
      const PlusAddressSettingService* setting_service,
      PlusAddressAllocator* allocator,
      url::Origin origin,
      std::string primary_email);
  ~PlusAddressSuggestionGenerator();

  // Returns the suggestions to be offered on the `focused_field` with Password
  // Manager classification `focused_form_classification`. `affiliated_profiles`
  // are assumed to be the plus profiles affiliated with the primary main frame
  // origin.
  [[nodiscard]] std::vector<autofill::Suggestion> GetSuggestions(
      const std::vector<std::string>& affiliated_plus_addresses,
      bool is_creation_enabled,
      const autofill::PasswordFormClassification& focused_form_classification,
      const autofill::FormFieldData& focused_field,
      autofill::AutofillSuggestionTriggerSource trigger_source);

  // Updates `suggestion` with a refreshed plus address by setting a new
  // payload.
  void RefreshPlusAddressForSuggestion(autofill::Suggestion& suggestion);

  // Returns a suggestion for managing plus addresses.
  static autofill::Suggestion GetManagePlusAddressSuggestion();

  // Returns a suggestion for displaying an error during plus address
  // reservation. The type of `error` determines which string to show and
  // whether to offer refresh.
  static autofill::Suggestion GetPlusAddressErrorSuggestion(
      const PlusAddressRequestError& error);

  // Updates `suggestion` to have `plus_address` as the proposed suggestions.
  // `CHECK`s that `suggestion` is of type `kCreateNewPlusAddressInline`.
  static void SetSuggestedPlusAddressForSuggestion(
      const PlusAddress& plus_address,
      autofill::Suggestion& suggestion);

  // Updates the `suggestion`'s style to indicate whether it `is_loading`.
  static void SetLoadingStateForSuggestion(bool is_loading,
                                           autofill::Suggestion& suggestion);

 private:
  // Returns a suggestion to create a new plus address.
  autofill::Suggestion CreateNewPlusAddressSuggestion();

  // Returns whether it is allowed to generate plus addresses inline. This is
  // true on Desktop platforms if the user has accepted the legal notice.
  bool IsInlineGenerationEnabled() const;

  // Returns a suggestion to generate a new plus address inline. If there are
  // pre-allocated plus addresses, it adds the next suggested plus address as
  // payload. Otherwise, the payload is left empty (and the UI will need to
  // request a suggested plus address on showing the suggestion).
  autofill::Suggestion CreateNewPlusAddressInlineSuggestion();

  const raw_ref<const PlusAddressSettingService> setting_service_;
  const raw_ref<PlusAddressAllocator> allocator_;
  // TODO(crbug.com/362445807): Eliminate this parameter once the allocator
  // no longer needs it.
  const url::Origin origin_;
  // The primary email address of the user.
  const std::string primary_email_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SUGGESTION_GENERATOR_H_
