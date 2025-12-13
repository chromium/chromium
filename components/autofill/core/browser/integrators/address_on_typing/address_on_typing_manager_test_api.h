// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager.h"

namespace autofill {

// Exposes some testing operations for AddressOnTypingManager.
class AddressOnTypingManagerTestApi {
 public:
  explicit AddressOnTypingManagerTestApi(AddressOnTypingManager* manager)
      : manager_(CHECK_DEREF(manager)) {}

  void AddStrikeToBlockAddressOnTypingSuggestions(FieldType field_type) {
    manager_->AddStrikeToBlockAddressOnTypingSuggestions(field_type);
  }

  std::optional<int> GetAddressOnTypingMaxStrikesLimit() const {
    return manager_->GetAddressOnTypingMaxStrikesLimit();
  }

  std::optional<int> GetAddressOnTypingFieldTypeStrikes(
      FieldType field_type) const {
    return manager_->GetAddressOnTypingFieldTypeStrikes(field_type);
  }

  void RemoveStrikesToBlockAddressOnTypingSuggestions(FieldType field_type) {
    manager_->RemoveStrikesToBlockAddressOnTypingSuggestions(field_type);
  }

 private:
  raw_ref<AddressOnTypingManager> manager_;
};

inline AddressOnTypingManagerTestApi test_api(AddressOnTypingManager& manager) {
  return AddressOnTypingManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ADDRESS_ON_TYPING_ADDRESS_ON_TYPING_MANAGER_TEST_API_H_
