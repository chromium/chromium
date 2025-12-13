// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {
namespace {

// Returns true if any of the features that use wallet public passes are
// enabled.
bool WalletPublicPassesEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillAiWalletVehicleRegistration) ||
         base::FeatureList::IsEnabled(
             features::kAutofillAiWalletFlightReservation);
}

// Creates a fallback icon used when there is no logo for loyalty card program.
// The icon consists of the first letter of the merchant name.
Suggestion::LetterMonochromeIcon CreateFallbackSuggestionIcon(
    std::string_view merchant_name) {
  CHECK(!merchant_name.empty());
  return Suggestion::LetterMonochromeIcon(
      base::UTF8ToUTF16(merchant_name.substr(0, 1)));
}

Suggestion CreateUndoOrClearFormSuggestion() {
#if BUILDFLAG(IS_IOS)
  // TODO(crbug.com/40266549): iOS still uses Clear Form logic, replace with
  // Undo.
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM),
      SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kClear;
#else
  std::u16string value = l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM);
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    value = base::i18n::ToUpper(value);
  }
  Suggestion suggestion(value, SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kUndo;
#endif
  // TODO(crbug.com/40266549): update "Clear Form" a11y announcement to "Undo"
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

// Set the URL for the loyalty card icon image or fallback icon to be shown in
// the `suggestion`.
void SetLoyaltyCardIconURL(Suggestion& suggestion,
                           const GURL& icon_url,
                           const ValuablesDataManager& valuables_manager,
                           std::string_view merchant_name) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    suggestion.custom_icon = Suggestion::CustomIconUrl(icon_url);
  } else {
    if (const gfx::Image* image =
            valuables_manager.GetCachedValuableImageForUrl(icon_url)) {
      suggestion.custom_icon = *image;
    } else {
      suggestion.custom_icon = CreateFallbackSuggestionIcon(merchant_name);
    }
  }
}

// Creates `Manage loyalty card` suggestion.
Suggestion CreateManageLoyaltyCardsSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS),
      SuggestionType::kManageLoyaltyCard);
  suggestion.voice_over =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS_A11Y_HINT);
  suggestion.icon = Suggestion::Icon::kSettings;
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
  SetLoyaltyCardIconURL(suggestion, loyalty_card.program_logo(),
                        valuables_manager, loyalty_card.merchant_name());
  if (WalletPublicPassesEnabled()) {
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillAiValuablesFeature);
  } else {
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature);
  }
  return suggestion;
}

// Creates suggestions from given `loyalty_cards` and adds them to given
// `suggestions`.
std::vector<Suggestion> CreateSuggestionsFromLoyaltyCards(
    base::span<const LoyaltyCard> loyalty_cards,
    const ValuablesDataManager& valuables_manager) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(loyalty_cards.size());
  for (const LoyaltyCard& loyalty_card : loyalty_cards) {
    suggestions.push_back(
        CreateLoyaltyCardSuggestion(loyalty_card, valuables_manager));
  }
  return suggestions;
}

// Returns non loyalty cards suggestions which are displayed below loyalty cards
// suggestions in the Autofill popup. `trigger_field_is_autofilled` is used to
// conditionally add suggestion for clearing autofilled field.
std::vector<Suggestion> GetLoyaltyCardsFooterSuggestions(
    bool trigger_field_is_autofilled) {
  std::vector<Suggestion> footer_suggestions;
  footer_suggestions.emplace_back(SuggestionType::kSeparator);
  if (trigger_field_is_autofilled) {
    footer_suggestions.push_back(CreateUndoOrClearFormSuggestion());
  }
  footer_suggestions.push_back(CreateManageLoyaltyCardsSuggestion());
  return footer_suggestions;
}

}  // namespace

std::vector<Suggestion> GetSuggestionsForLoyaltyCards(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client) {
  if (!client.GetValuablesDataManager()) {
    return {};
  }
  std::vector<Suggestion> suggestions;
  LoyaltyCardSuggestionGenerator loyalty_card_suggestion_generator;

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  auto on_suggestion_data_returned =
      [&on_suggestions_generated, &form, &field, &form_structure,
       &autofill_field, &client, &loyalty_card_suggestion_generator](
          std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>>
              suggestion_data) {
        loyalty_card_suggestion_generator.GenerateSuggestions(
            form, field, form_structure, autofill_field, client,
            {std::move(suggestion_data)}, on_suggestions_generated);
      };

  // Since the `on_suggestions_generated` callback is called synchronously,
  // we can assume that `suggestions` will hold correct value.
  loyalty_card_suggestion_generator.FetchSuggestionData(
      form, field, form_structure, autofill_field, client,
      on_suggestion_data_returned);
  return suggestions;
}

void ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
    const ValuablesDataManager& valuables_manager,
    const GURL& url,
    bool trigger_field_is_autofilled,
    std::vector<Suggestion>& email_suggestions) {
  std::vector<LoyaltyCard> all_loyalty_cards =
      valuables_manager.GetLoyaltyCardsToSuggest();
  CHECK(!email_suggestions.empty());
  if (all_loyalty_cards.empty()) {
    return;
  }
  std::vector<LoyaltyCard> affiliated_cards;
  std::copy_if(all_loyalty_cards.begin(), all_loyalty_cards.end(),
               std::back_inserter(affiliated_cards),
               [&](const LoyaltyCard& card) {
                 return card.GetAffiliationCategory(url) ==
                        LoyaltyCard::AffiliationCategory::kAffiliated;
               });
  if (affiliated_cards.empty()) {
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // No submenu on Android. Loyalty card suggestions are listed right after
  // email suggestions.
  std::vector<Suggestion> loyalty_card_suggestions =
      CreateSuggestionsFromLoyaltyCards(affiliated_cards, valuables_manager);
  email_suggestions.insert(
      email_suggestions.end(),
      std::make_move_iterator(loyalty_card_suggestions.begin()),
      std::make_move_iterator(loyalty_card_suggestions.end()));
  return;
#else
  Suggestion submenu_suggestion = Suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_LOYALTY_CARDS_SUBMENU_TITLE),
      SuggestionType::kAllLoyaltyCardsEntry);
  submenu_suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  submenu_suggestion.icon = Suggestion::Icon::kGoogleWalletMonochrome;
#endif
  submenu_suggestion.children =
      CreateSuggestionsFromLoyaltyCards(affiliated_cards, valuables_manager);
  submenu_suggestion.children.emplace_back(SuggestionType::kSeparator);
  submenu_suggestion.children.emplace_back(
      CreateManageLoyaltyCardsSuggestion());
  // There is at least one email, separator and manage addresses suggestion.
  CHECK_GE(email_suggestions.size(), 3u);
  if (trigger_field_is_autofilled) {
    CHECK_EQ(email_suggestions[email_suggestions.size() - 2].type,
             SuggestionType::kUndoOrClear);
    // If the field is autofilled, insert the submenu suggestion before undo and
    // Manage address suggestions.
    email_suggestions.insert(email_suggestions.end() - 2, submenu_suggestion);
    email_suggestions.insert(email_suggestions.end() - 2,
                             Suggestion(SuggestionType::kSeparator));
  } else {
    // If the field is not yet autofilled, insert the submenu suggestion before
    // the Manage address suggestion.
    email_suggestions.insert(email_suggestions.end() - 1, submenu_suggestion);
    email_suggestions.insert(email_suggestions.end() - 1,
                             Suggestion(SuggestionType::kSeparator));
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

LoyaltyCardSuggestionGenerator::LoyaltyCardSuggestionGenerator() = default;

LoyaltyCardSuggestionGenerator::~LoyaltyCardSuggestionGenerator() = default;

void LoyaltyCardSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](std::pair<SuggestionDataSource,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void LoyaltyCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void LoyaltyCardSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!trigger_autofill_field || !client.GetValuablesDataManager() ||
      trigger_autofill_field->Type().GetTypes().contains_none(
          {LOYALTY_MEMBERSHIP_ID, EMAIL_OR_LOYALTY_MEMBERSHIP_ID})) {
    callback({SuggestionDataSource::kLoyaltyCard, {}});
    return;
  }

  if (SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field)) {
    callback({SuggestionDataSource::kLoyaltyCard, {}});
    return;
  }

  std::vector<LoyaltyCard> loyalty_cards =
      client.GetValuablesDataManager()->GetLoyaltyCardsToSuggest();
  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(loyalty_cards),
      [](LoyaltyCard& card) { return SuggestionData(std::move(card)); });
  callback({SuggestionDataSource::kLoyaltyCard, std::move(suggestion_data)});
}

void LoyaltyCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  auto it = all_suggestion_data.find(SuggestionDataSource::kLoyaltyCard);
  std::vector<SuggestionData> loyalty_card_suggestion_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();

  if (!client.GetValuablesDataManager() ||
      loyalty_card_suggestion_data.empty()) {
    callback({FillingProduct::kLoyaltyCard, {}});
    return;
  }
  std::vector<LoyaltyCard> all_loyalty_cards =
      base::ToVector(std::move(loyalty_card_suggestion_data),
                     [](SuggestionData& suggestion_data) {
                       return std::get<LoyaltyCard>(std::move(suggestion_data));
                     });

  auto non_affiliated_cards = std::ranges::stable_partition(
      all_loyalty_cards, [&](const LoyaltyCard& card) {
        return card.GetAffiliationCategory(
                   client.GetLastCommittedPrimaryMainFrameURL()) ==
               LoyaltyCard::AffiliationCategory::kAffiliated;
      });
  // SAFETY: Bounds information contained in vector iterators.
  UNSAFE_BUFFERS(std::vector<LoyaltyCard> affiliated_cards(
      all_loyalty_cards.begin(), non_affiliated_cards.begin()));

  // Show suggestions only in case there is a card that matches current domain.
  if (affiliated_cards.empty()) {
    callback({FillingProduct::kLoyaltyCard, {}});
    return;
  }

  // If no submenu is needed.
#if BUILDFLAG(IS_ANDROID)
  const bool generate_flat_suggestions = true;
#else
  const bool generate_flat_suggestions = non_affiliated_cards.empty();
#endif

  if (generate_flat_suggestions) {
    std::vector<Suggestion> suggestions = CreateSuggestionsFromLoyaltyCards(
        affiliated_cards, *client.GetValuablesDataManager());
    std::ranges::move(
        GetLoyaltyCardsFooterSuggestions(trigger_field.is_autofilled()),
        std::back_inserter(suggestions));
    callback({FillingProduct::kLoyaltyCard, std::move(suggestions)});
    return;
  }

  // Build suggestions with 'all loyalty cards' submenu.
  std::vector<Suggestion> suggestions = CreateSuggestionsFromLoyaltyCards(
      affiliated_cards, *client.GetValuablesDataManager());
  suggestions.emplace_back(SuggestionType::kSeparator);

  // Build 'all loyalty cards' submenu.
  Suggestion& submenu_suggestion = suggestions.emplace_back(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOYALTY_CARDS_ALL_YOUR_CARDS_SUBMENU_TITLE),
      SuggestionType::kAllLoyaltyCardsEntry);
  submenu_suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  submenu_suggestion.icon = Suggestion::Icon::kGoogleWalletMonochrome;
#endif
  submenu_suggestion.children = CreateSuggestionsFromLoyaltyCards(
      client.GetValuablesDataManager()->GetLoyaltyCardsToSuggest(),
      *client.GetValuablesDataManager());
  std::ranges::move(
      GetLoyaltyCardsFooterSuggestions(trigger_field.is_autofilled()),
      std::back_inserter(suggestions));
  callback({FillingProduct::kLoyaltyCard, std::move(suggestions)});
}

}  // namespace autofill
