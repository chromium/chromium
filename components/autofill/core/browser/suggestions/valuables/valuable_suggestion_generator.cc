// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"
namespace autofill {

namespace {

// Set the URL for the loyalty card icon image to be shown in the `suggestion`.
void SetIconURL(Suggestion& suggestion,
                const GURL& icon_url,
                const ValuablesDataManager& valuables_manager) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    suggestion.custom_icon = Suggestion::CustomIconUrl(icon_url);
  } else {
    // TODO(crbug.com/404437008): Check that the pointer is always valid once a
    // default icon is available.
    if (const gfx::Image* image =
            valuables_manager.GetCachedValuableImageForUrl(icon_url)) {
      suggestion.custom_icon = *image;
    }
  }
}

Suggestion CreateManageLoyaltyCardsSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS),
      SuggestionType::kManageLoyaltyCard);
  suggestion.icon = Suggestion::Icon::kSettings;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  suggestion.trailing_icon = Suggestion::Icon::kGoogleWallet;
#endif
  return suggestion;
}

// Builds suggestion for given `loyalty_card`.
Suggestion CreateLoyaltyCardSuggestion(
    const LoyaltyCard& loyalty_card,
    const ValuablesDataManager& valuables_manager) {
  Suggestion suggestion =
      Suggestion(base::UTF8ToUTF16(loyalty_card.loyalty_card_number()),
                 SuggestionType::kLoyaltyCardEntry);
  suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  std::u16string merchant_name =
      base::UTF8ToUTF16(loyalty_card.merchant_name());
  suggestion.labels.push_back({Suggestion::Text(merchant_name)});
  suggestion.payload = Suggestion::Guid(loyalty_card.id().value());
  SetIconURL(suggestion, loyalty_card.program_logo(), valuables_manager);
#if !BUILDFLAG(IS_ANDROID)
  // The IPH is only available on Desktop.
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      &feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature);
#endif  // BUILDFLAG(IS_ANDROID)
  return suggestion;
}

// Returns whether given `loyalty_card` any of merchant domains match given
// `url`.
bool LoyaltyCardMatchesDomain(const LoyaltyCard& loyalty_card,
                              const GURL& url) {
  return std::ranges::any_of(
      loyalty_card.merchant_domains(), [url](const GURL& merchant_url) {
        return affiliations::IsExtendedPublicSuffixDomainMatch(merchant_url,
                                                               url, {});
      });
}

}  // namespace

std::vector<Suggestion> GetLoyaltyCardSuggestions(
    const ValuablesDataManager& valuables_manager,
    const GURL& url) {
  const base::span<const LoyaltyCard> loyalty_cards =
      valuables_manager.GetLoyaltyCards();
  if (loyalty_cards.empty()) {
    return {};
  }

  std::vector<LoyaltyCard> partitionable_cards(loyalty_cards.begin(),
                                               loyalty_cards.end());
  std::ranges::sort(partitionable_cards,
                    [](const LoyaltyCard& a, const LoyaltyCard& b) {
                      return a.merchant_name() < b.merchant_name();
                    });
  auto non_affiliated_cards = std::ranges::stable_partition(
      partitionable_cards, [&](const LoyaltyCard& card) {
        return LoyaltyCardMatchesDomain(card, url);
      });

  // SAFETY: Bounds information contained in vector iterators.
  UNSAFE_BUFFERS(base::span<LoyaltyCard> affiliated_cards(
      partitionable_cards.begin(), non_affiliated_cards.begin()));
  std::vector<Suggestion> suggestions;
  // Build matching loyalty cards top suggestions.
  for (const LoyaltyCard& loyalty_card : affiliated_cards) {
    suggestions.push_back(
        CreateLoyaltyCardSuggestion(loyalty_card, valuables_manager));
  }

  // If there was at least one matching loyalty card add a separator.
  if (!affiliated_cards.empty()) {
    suggestions.emplace_back(SuggestionType::kSeparator);
  }

  // Build remaining loyalty cards suggestions.
  for (const LoyaltyCard& loyalty_card : non_affiliated_cards) {
    suggestions.push_back(
        CreateLoyaltyCardSuggestion(loyalty_card, valuables_manager));
  }

  // If there was at least one non-matching loyalty card add a separator.
  if (!non_affiliated_cards.empty()) {
    suggestions.emplace_back(SuggestionType::kSeparator);
  }

  // Add 'manage loyalty cards' suggestion.
  suggestions.push_back(CreateManageLoyaltyCardsSuggestion());
  return suggestions;
}

void ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
    std::vector<Suggestion>& email_suggestions,
    const ValuablesDataManager& valuables_manager,
    const GURL& url) {
  std::vector<Suggestion> loyalty_card_suggestions =
      GetLoyaltyCardSuggestions(valuables_manager, url);
  if (loyalty_card_suggestions.empty()) {
    return;
  }
  if (email_suggestions.empty()) {
    email_suggestions.insert(
        email_suggestions.end(),
        std::make_move_iterator(loyalty_card_suggestions.begin()),
        std::make_move_iterator(loyalty_card_suggestions.end()));
    return;
  }

  Suggestion submenu_suggestion = Suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_LOYALTY_CARDS_SUBMENU_TITLE),
      SuggestionType::kLoyaltyCardEntry);
  submenu_suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  submenu_suggestion.children = loyalty_card_suggestions;

  // There is at least one email, separator and manage addresses suggestion.
  CHECK(email_suggestions.size() >= 3);
  email_suggestions.insert(email_suggestions.end() - 1, submenu_suggestion);
  email_suggestions.insert(email_suggestions.end() - 1,
                           Suggestion(SuggestionType::kSeparator));
}

}  // namespace autofill
