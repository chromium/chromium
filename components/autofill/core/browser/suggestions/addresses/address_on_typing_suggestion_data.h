// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_ON_TYPING_SUGGESTION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_ON_TYPING_SUGGESTION_DATA_H_

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Used to hold the relevant data for address on typing suggestions between
// fetching and generating phases of the suggestion generation.
struct AddressOnTypingSuggestionData {
  std::u16string suggestion_text;
  FieldType type;
  std::string guid;

  bool operator==(const AddressOnTypingSuggestionData& other) const = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ADDRESSES_ADDRESS_ON_TYPING_SUGGESTION_DATA_H_
