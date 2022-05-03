// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/autofill/core/browser/autofill_suggestion_generator.h"

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_selection.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Returns the credit card field |value| trimmed from whitespace and with stop
// characters removed.
std::u16string SanitizeCreditCardFieldValue(const std::u16string& value) {
  std::u16string sanitized;
  // We remove whitespace as well as some invisible unicode characters.
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized);
  base::TrimString(sanitized,
                   std::u16string({base::i18n::kRightToLeftMark,
                                   base::i18n::kLeftToRightMark}),
                   &sanitized);
  // Some sites have ____-____-____-____ in their credit card number fields, for
  // example.
  base::RemoveChars(sanitized, u"-_", &sanitized);
  return sanitized;
}

}  // namespace

AutofillSuggestionGenerator::AutofillSuggestionGenerator(
    AutofillClient* autofill_client,
    PersonalDataManager* personal_data)
    : autofill_client_(autofill_client), personal_data_(personal_data) {}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetSuggestionsForCreditCards(
    const FormStructure& form_structure,
    const FormFieldData& field,
    const AutofillType& type,
    const std::string& app_locale) {
  std::vector<Suggestion> suggestions;

  DCHECK(personal_data_);
  std::vector<CreditCard*> cards_to_suggest =
      personal_data_->GetCreditCardsToSuggest(
          autofill_client_->AreServerCardsSupported());

  // The field value is sanitized before attempting to match it to the user's
  // data.
  auto field_contents = SanitizeCreditCardFieldValue(field.value);

  // Suppress disused credit cards when triggered from an empty field.
  if (field_contents.empty()) {
    const base::Time min_last_used =
        AutofillClock::Now() - kDisusedDataModelTimeDelta;
    RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        AutofillClock::Now(), min_last_used, &cards_to_suggest);
  }

  std::u16string field_contents_lower = base::i18n::ToLower(field_contents);

  for (const CreditCard* credit_card : cards_to_suggest) {
    // The value of the stored data for this field type in the |credit_card|.
    std::u16string creditcard_field_value =
        credit_card->GetInfo(type, app_locale);
    if (creditcard_field_value.empty())
      continue;

    bool prefix_matched_suggestion;
    if (suggestion_selection::IsValidSuggestionForFieldContents(
            base::i18n::ToLower(creditcard_field_value), field_contents_lower,
            type, credit_card->record_type() == CreditCard::MASKED_SERVER_CARD,
            field.is_autofilled, &prefix_matched_suggestion)) {
      if (ShouldShowVirtualCardOption(credit_card, form_structure)) {
        suggestions.push_back(CreateCreditCardSuggestion(
            *credit_card, type, prefix_matched_suggestion,
            /*virtual_card_option=*/true, app_locale));
      }

      suggestions.push_back(CreateCreditCardSuggestion(
          *credit_card, type, prefix_matched_suggestion,
          /*virtual_card_option=*/false, app_locale));
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  // AutofillOfferManager will populate the offer text into the suggestion if
  // the suggestion represents a credit card that has activated offers.
  AutofillOfferManager* offer_manager =
      autofill_client_->GetAutofillOfferManager();
  if (offer_manager) {
    offer_manager->UpdateSuggestionsWithOffers(
        autofill_client_->GetLastCommittedURL(), suggestions);
  }

  return suggestions;
}

// static
void AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
    base::Time comparison_time,
    base::Time min_last_used,
    std::vector<CreditCard*>* cards) {
  const size_t original_size = cards->size();
  // Split the vector into two groups
  // 1. All server cards, unexpired local cards, or local cards that have been
  // used after |min_last_used|;
  // 2. Expired local cards that have not been used since |min_last_used|;
  // then delete the latter.
  cards->erase(std::stable_partition(
                   cards->begin(), cards->end(),
                   [comparison_time, min_last_used](const CreditCard* c) {
                     return !c->IsExpired(comparison_time) ||
                            c->use_date() >= min_last_used ||
                            c->record_type() != CreditCard::LOCAL_CARD;
                   }),
               cards->end());
  const size_t num_cards_supressed = original_size - cards->size();
  AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
      num_cards_supressed);
}

std::u16string AutofillSuggestionGenerator::GetDisplayNicknameForCreditCard(
    const CreditCard& card) const {
  // Always prefer a local nickname if available.
  if (card.HasNonEmptyValidNickname() &&
      card.record_type() == CreditCard::LOCAL_CARD)
    return card.nickname();
  // Either the card a) has no nickname or b) is a server card and we would
  // prefer to use the nickname of a local card.
  std::vector<CreditCard*> candidates = personal_data_->GetCreditCards();
  for (CreditCard* candidate : candidates) {
    if (candidate->guid() != card.guid() && candidate->HasSameNumberAs(card) &&
        candidate->HasNonEmptyValidNickname()) {
      return candidate->nickname();
    }
  }
  // Fall back to nickname of |card|, which may be empty.
  return card.nickname();
}

Suggestion AutofillSuggestionGenerator::CreateCreditCardSuggestion(
    const CreditCard& credit_card,
    const AutofillType& type,
    bool prefix_matched_suggestion,
    bool virtual_card_option,
    const std::string& app_locale) const {
  Suggestion suggestion;

  suggestion.main_text = Suggestion::Text(credit_card.GetInfo(type, app_locale),
                                          Suggestion::Text::IsPrimary(true));
  suggestion.icon = credit_card.CardIconStringForAutofillSuggestion();
  std::string backend_id = credit_card.guid();
  suggestion.match = prefix_matched_suggestion ? Suggestion::PREFIX_MATCH
                                               : Suggestion::SUBSTRING_MATCH;

  GURL card_art_url_for_virtual_card_option;
  if (virtual_card_option &&
      credit_card.record_type() == CreditCard::MASKED_SERVER_CARD) {
    card_art_url_for_virtual_card_option = credit_card.card_art_url();
  } else if (virtual_card_option &&
             credit_card.record_type() == CreditCard::LOCAL_CARD) {
    const CreditCard* server_duplicate_card =
        GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    card_art_url_for_virtual_card_option =
        server_duplicate_card->card_art_url();
    backend_id = server_duplicate_card->guid();
  }
  suggestion.backend_id = backend_id;

  // Get the nickname for the card suggestion, which may not be the same as
  // the card's nickname if there are duplicates of the card on file.
  std::u16string suggestion_nickname =
      GetDisplayNicknameForCreditCard(credit_card);

  // The kAutofillKeyboardAccessory feature is only available on Android. So for
  // other platforms, we'd always use the obfuscation_length of 4.
  int obfuscation_length =
      base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory) ? 2
                                                                         : 4;
  // If the value is the card number, the label is the expiration date.
  // Otherwise the label is the card number, or if that is empty the
  // cardholder name. The label should never repeat the value.
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    suggestion.main_text =
        Suggestion::Text(credit_card.CardIdentifierStringForAutofillDisplay(
                             suggestion_nickname, obfuscation_length),
                         Suggestion::Text::IsPrimary(true));

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    suggestion.label = credit_card.GetInfo(
        AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale);
#else
    suggestion.label = credit_card.DescriptiveExpiration(app_locale);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  } else if (credit_card.number().empty()) {
    DCHECK_EQ(credit_card.record_type(), CreditCard::LOCAL_CARD);
    if (credit_card.HasNonEmptyValidNickname()) {
      suggestion.label = credit_card.nickname();
    } else if (type.GetStorableType() != CREDIT_CARD_NAME_FULL) {
      suggestion.label =
          credit_card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), app_locale);
    }
  } else {
#if BUILDFLAG(IS_ANDROID)
    // On Android devices, the label is formatted as
    // "Nickname/Network  ••••1234" when the keyboard accessory experiment
    // is disabled and as "••1234" when it's enabled.
    suggestion.label =
        base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory)
            ? credit_card.ObfuscatedLastFourDigits(obfuscation_length)
            : credit_card.CardIdentifierStringForAutofillDisplay(
                  suggestion_nickname);
#elif BUILDFLAG(IS_IOS)
    // E.g. "••••1234"".
    suggestion.label = credit_card.ObfuscatedLastFourDigits();
#else
    // E.g. "Nickname/Network  ••••1234, expires on 01/25".
    suggestion.label =
        credit_card.CardIdentifierStringAndDescriptiveExpiration(app_locale);
#endif
  }

  if (virtual_card_option) {
#if BUILDFLAG(IS_ANDROID)
    suggestion.custom_icon_url = credit_card.card_art_url();
#endif  // BUILDFLAG(IS_ANDROID)

    suggestion.frontend_id = POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY;
    suggestion.minor_text.value = suggestion.main_text.value;
    suggestion.main_text.value = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);

    suggestion.feature_for_iph =
        feature_engagement::kIPHAutofillVirtualCardSuggestionFeature.name;

    gfx::Image* image = personal_data_->GetCreditCardArtImageForUrl(
        card_art_url_for_virtual_card_option);
    if (image)
      suggestion.custom_icon = *image;
  }

  return suggestion;
}

bool AutofillSuggestionGenerator::ShouldShowVirtualCardOption(
    const CreditCard* candidate_card,
    const FormStructure& form_structure) const {
  // If virtual card experiment is disabled:
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableMerchantBoundVirtualCards)) {
    return false;
  }

  // If the form is an incomplete form and the incomplete form experiment is
  // disabled, do not offer a virtual card option. We will likely not be able to
  // fill in all information, and the user doesn't have the info either.
  if (!IsCompleteCreditCardFormIncludingCvcField(form_structure) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillSuggestVirtualCardsOnIncompleteForm)) {
    return false;
  }

  switch (candidate_card->record_type()) {
    case CreditCard::MASKED_SERVER_CARD:
      return candidate_card->virtual_card_enrollment_state() ==
             CreditCard::ENROLLED;
    case CreditCard::LOCAL_CARD: {
      const CreditCard* server_duplicate =
          GetServerCardForLocalCard(candidate_card);
      return server_duplicate &&
             server_duplicate->virtual_card_enrollment_state() ==
                 CreditCard::ENROLLED;
    }
    case CreditCard::FULL_SERVER_CARD:
      return false;
    case CreditCard::VIRTUAL_CARD:
      // Should not happen since virtual card is not persisted.
      NOTREACHED();
      return false;
  }
}

const CreditCard* AutofillSuggestionGenerator::GetServerCardForLocalCard(
    const CreditCard* local_card) const {
  DCHECK(local_card);
  if (local_card->record_type() != CreditCard::LOCAL_CARD)
    return nullptr;

  std::vector<CreditCard*> server_cards =
      personal_data_->GetServerCreditCards();
  auto it = base::ranges::find_if(
      server_cards.begin(), server_cards.end(),
      [&](const CreditCard* server_card) {
        return local_card->IsLocalDuplicateOfServerCard(*server_card);
      });

  if (it != server_cards.end())
    return *it;

  return nullptr;
}

}  // namespace autofill
