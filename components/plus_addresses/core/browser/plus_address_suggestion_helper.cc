// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/plus_address_suggestion_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {

namespace {

using autofill::FormFieldData;
using autofill::Suggestion;
using autofill::SuggestionType;

// Returns a suggestion to fill an existing plus address.
Suggestion CreateFillPlusAddressSuggestion(std::u16string plus_address) {
  Suggestion suggestion = Suggestion(std::move(plus_address),
                                     SuggestionType::kFillExistingPlusAddress);
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  return suggestion;
}

}  // namespace

PlusAddressSuggestionHelper::PlusAddressSuggestionHelper() = default;

PlusAddressSuggestionHelper::~PlusAddressSuggestionHelper() = default;

std::vector<autofill::Suggestion> PlusAddressSuggestionHelper::GetSuggestions(
    const std::vector<std::string>& affiliated_plus_addresses) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_plus_addresses.size());
  for (const std::string& affiliated_plus_address : affiliated_plus_addresses) {
    suggestions.push_back(CreateFillPlusAddressSuggestion(
        base::UTF8ToUTF16(affiliated_plus_address)));
  }
  // It is required by `autofill::SuggestionGenerator` that this function should
  // not filter plus addresses and should return an `autofill::Suggestion`
  // object for each of them.
  CHECK_EQ(suggestions.size(), affiliated_plus_addresses.size());
  return suggestions;
}

// static
Suggestion PlusAddressSuggestionHelper::GetManagePlusAddressSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

}  // namespace plus_addresses
