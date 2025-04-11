// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"

namespace autofill {

namespace {

Suggestion CreateManageLoyaltyCardsSuggestion() {
  // TODO(crbug.com/404436027): Add i18n, replace with:
  // l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS)
  Suggestion suggestion(u"Manage loyalty cards...",
                        SuggestionType::kManageLoyaltyCard);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

}  // namespace

std::vector<Suggestion> GetLoyaltyCardSuggestions(
    const base::span<const LoyaltyCard> loyalty_cards) {
  if (loyalty_cards.empty()) {
    return {};
  }
  std::vector<Suggestion> suggestions;
  for (const LoyaltyCard& loyalty_card : loyalty_cards) {
    Suggestion& suggestion = suggestions.emplace_back(
        base::UTF8ToUTF16(loyalty_card.loyalty_card_number()),
        SuggestionType::kLoyaltyCardEntry);
    suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(true);
    std::u16string merchant_name =
        base::UTF8ToUTF16(loyalty_card.merchant_name());
    suggestion.labels.push_back({Suggestion::Text(merchant_name)});
    suggestion.payload = Suggestion::Guid(loyalty_card.id().value());
  }
  suggestions.emplace_back(SuggestionType::kSeparator);
  suggestions.push_back(CreateManageLoyaltyCardsSuggestion());
  return suggestions;
}

}  // namespace autofill
