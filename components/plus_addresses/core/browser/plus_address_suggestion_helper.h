// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "url/origin.h"

namespace autofill {
struct Suggestion;
}  // namespace autofill

namespace plus_addresses {

class PlusAddressAllocator;
class PlusAddressSettingService;

// Helper class for generation plus address suggestions. Objects of this class
// are not intended to be saved into a member - instead, their lifetime should
// be scoped to a method call that generates suggestions.
class PlusAddressSuggestionHelper final {
  STACK_ALLOCATED();

 public:
  PlusAddressSuggestionHelper(const PlusAddressSettingService* setting_service,
                              PlusAddressAllocator* allocator,
                              url::Origin origin);
  ~PlusAddressSuggestionHelper();

  // Returns the suggestions to be offered on the field in `focused_form` with
  // `focused_field_id` with Password Manager classification
  // `focused_form_classification`. `affiliated_profiles` are assumed to be the
  // plus profiles affiliated with the primary main frame origin.
  // Note that the method CHECKs that a field with `focused_field_id` is
  // contained in `focused_form`.
  [[nodiscard]] std::vector<autofill::Suggestion> GetSuggestions(
      const std::vector<std::string>& affiliated_plus_addresses,
      const autofill::FormFieldData& focused_field,
      bool is_plus_address_manually_triggered);

  // Returns a suggestion for managing plus addresses.
  static autofill::Suggestion GetManagePlusAddressSuggestion();

  // Updates `suggestion` to have `plus_address` as the proposed suggestions.
  // `CHECK`s that `suggestion` is of type `kCreateNewPlusAddressInline`.
  static void SetSuggestedPlusAddressForSuggestion(
      const PlusAddress& plus_address,
      autofill::Suggestion& suggestion);

  // Updates the `suggestion`'s style to indicate whether it `is_loading`.
  static void SetLoadingStateForSuggestion(bool is_loading,
                                           autofill::Suggestion& suggestion);

 private:

  // Returns whether it is allowed to generate plus addresses inline. This is
  // true on Desktop platforms if the user has accepted the legal notice.
  bool IsInlineGenerationEnabled() const;

  const raw_ref<const PlusAddressSettingService> setting_service_;
  const raw_ref<PlusAddressAllocator> allocator_;
  // TODO(crbug.com/362445807): Eliminate this parameter once the allocator
  // no longer needs it.
  const url::Origin origin_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_
