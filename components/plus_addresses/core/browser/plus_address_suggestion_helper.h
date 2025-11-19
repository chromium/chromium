// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/stack_allocated.h"

namespace autofill {
struct Suggestion;
}  // namespace autofill

namespace plus_addresses {

// Helper class for generation plus address suggestions. Objects of this class
// are not intended to be saved into a member - instead, their lifetime should
// be scoped to a method call that generates suggestions.
// TODO(crbug.com/409962888): Remove this class and make the methods free
// functions.
class PlusAddressSuggestionHelper final {
  STACK_ALLOCATED();

 public:
  PlusAddressSuggestionHelper();
  ~PlusAddressSuggestionHelper();

  // Returns the suggestions to be offered for `affiliated_plus_addresses`.
  // Note that this method does not do any filtering and always returns
  // suggestions for all plus addresses in `affiliated_plus_addresses`.
  [[nodiscard]] std::vector<autofill::Suggestion> GetSuggestions(
      const std::vector<std::string>& affiliated_plus_addresses);

  // Returns a suggestion for managing plus addresses.
  static autofill::Suggestion GetManagePlusAddressSuggestion();
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_
