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
    const std::vector<std::string>& affiliated_plus_addresses,
    const autofill::FormFieldData& focused_field,
    bool is_plus_address_manually_triggered) {
  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field.value());

  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_plus_addresses.size());
  for (const std::string& affiliated_plus_address : affiliated_plus_addresses) {
    std::u16string plus_address = base::UTF8ToUTF16(affiliated_plus_address);
    // Generally, plus address suggestions are only available on fields whose
    // content matches the suggestion text. In cases where the field was
    // previously autofilled or suggestions were manually triggered, no prefix
    // matching should be applied.
    // TODO(crbug.com/409962888): Remove filtering and add a CHECK once
    // `BrowserAutofillManager` doesn't call
    // PlusAddressServiceImpl::GetSuggestionsFromPlusAddresses() anymore.
    if (is_plus_address_manually_triggered || focused_field.is_autofilled() ||
        plus_address.starts_with(normalized_field_value)) {
      suggestions.push_back(
          CreateFillPlusAddressSuggestion(std::move(plus_address)));
    }
  }
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
