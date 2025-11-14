// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/stack_allocated.h"

namespace autofill {
class FormFieldData;
struct Suggestion;
}  // namespace autofill

namespace plus_addresses {

// Helper class for generation plus address suggestions. Objects of this class
// are not intended to be saved into a member - instead, their lifetime should
// be scoped to a method call that generates suggestions.
class PlusAddressSuggestionHelper final {
  STACK_ALLOCATED();

 public:
  PlusAddressSuggestionHelper();
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
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SUGGESTION_HELPER_H_
