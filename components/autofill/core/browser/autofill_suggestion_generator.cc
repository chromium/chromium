// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_suggestion_generator.h"

#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_filler.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
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

// Returns the card-linked offers map with credit card guid as the key and the
// pointer to the linked AutofillOfferData as the value.
std::map<std::string, AutofillOfferData*> getCardLinkedOffers(
    AutofillClient* autofill_client) {
  AutofillOfferManager* offer_manager =
      autofill_client->GetAutofillOfferManager();
  if (offer_manager) {
    return offer_manager->GetCardLinkedOffersMap(
        autofill_client->GetLastCommittedPrimaryMainFrameURL());
  }
  return {};
}

}  // namespace

AutofillSuggestionGenerator::AutofillSuggestionGenerator(
    AutofillClient* autofill_client,
    PersonalDataManager* personal_data)
    : autofill_client_(autofill_client), personal_data_(personal_data) {}

AutofillSuggestionGenerator::~AutofillSuggestionGenerator() = default;

std::vector<Suggestion> AutofillSuggestionGenerator::GetSuggestionsForProfiles(
    const FormStructure& form,
    const FormFieldData& field,
    const AutofillField& autofill_field,
    const std::string& app_locale) {
  std::vector<ServerFieldType> field_types(form.field_count());
  for (size_t i = 0; i < form.field_count(); ++i) {
    field_types.push_back(form.field(i)->Type().GetStorableType());
  }

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      autofill_field.Type(), field.value, field.is_autofilled, field_types);

  // Adjust phone number to display in prefix/suffix case.
  if (autofill_field.Type().group() == FieldTypeGroup::kPhoneHome) {
    for (auto& suggestion : suggestions) {
      const AutofillProfile* profile = personal_data_->GetProfileByGUID(
          suggestion.GetPayload<Suggestion::BackendId>().value());
      if (profile) {
        const std::u16string phone_home_city_and_number =
            profile->GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale);
        suggestion.main_text =
            Suggestion::Text(FieldFiller::GetPhoneNumberValueForInput(
                                 autofill_field, suggestion.main_text.value,
                                 phone_home_city_and_number, field),
                             Suggestion::Text::IsPrimary(true));
      }
    }
  }

  for (auto& suggestion : suggestions) {
    suggestion.frontend_id =
        MakeFrontendId(Suggestion::BackendId(),
                       suggestion.GetPayload<Suggestion::BackendId>());
  }

  return suggestions;
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetSuggestionsForCreditCards(
    const FormFieldData& field,
    const AutofillType& type,
    const std::string& app_locale,
    bool* should_display_gpay_logo,
    bool* with_offer) {
  std::vector<Suggestion> suggestions;

  std::map<std::string, AutofillOfferData*> card_linked_offers_map =
      getCardLinkedOffers(autofill_client_);
  *with_offer = !card_linked_offers_map.empty();

  DCHECK(personal_data_);
  std::vector<CreditCard*> cards_to_suggest =
      personal_data_->GetCreditCardsToSuggest(
          autofill_client_->AreServerCardsSupported());

  // If a card has available card linked offers on the last committed url, rank
  // it to the top.
  if (!card_linked_offers_map.empty()) {
    base::ranges::stable_sort(
        cards_to_suggest,
        [&card_linked_offers_map](const CreditCard* a, const CreditCard* b) {
          return base::Contains(card_linked_offers_map, a->guid()) &&
                 !base::Contains(card_linked_offers_map, b->guid());
        });
  }

  *should_display_gpay_logo = base::ranges::all_of(
      cards_to_suggest, base::not_fn(&CreditCard::IsLocalCard));

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
      bool card_linked_offer_available =
          base::Contains(card_linked_offers_map, credit_card->guid());
      if (ShouldShowVirtualCardOption(credit_card)) {
        suggestions.push_back(CreateCreditCardSuggestion(
            *credit_card, type, prefix_matched_suggestion,
            /*virtual_card_option=*/true, app_locale,
            card_linked_offer_available));
      }
      suggestions.push_back(CreateCreditCardSuggestion(
          *credit_card, type, prefix_matched_suggestion,
          /*virtual_card_option=*/false, app_locale,
          card_linked_offer_available));
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  for (Suggestion& suggestion : suggestions) {
    if (suggestion.frontend_id == 0) {
      suggestion.frontend_id =
          MakeFrontendId(suggestion.GetPayload<Suggestion::BackendId>(),
                         Suggestion::BackendId());
    }
  }

  return suggestions;
}

// static
std::vector<Suggestion> AutofillSuggestionGenerator::GetSuggestionsForIBANs(
    const std::vector<IBAN*>& ibans) {
  std::vector<Suggestion> suggestions;
  for (const IBAN* iban : ibans) {
    Suggestion& suggestion = suggestions.emplace_back(iban->value());
    suggestion.frontend_id = POPUP_ITEM_ID_IBAN_ENTRY;
    suggestion.payload = Suggestion::BackendId(iban->guid());
    suggestion.main_text.value = iban->GetIdentifierStringForAutofillDisplay();
    if (!iban->nickname().empty())
      suggestion.labels = {{Suggestion::Text(iban->nickname())}};
  }
  return suggestions;
}

// static
std::vector<Suggestion>
AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
    const std::vector<const AutofillOfferData*>& promo_code_offers) {
  std::vector<Suggestion> suggestions;
  GURL footer_offer_details_url;
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    // For each promo code, create a suggestion.
    suggestions.emplace_back(
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode()));
    Suggestion& suggestion = suggestions.back();
    if (!promo_code_offer->GetDisplayStrings().value_prop_text.empty()) {
      suggestion.labels = {{Suggestion::Text(base::ASCIIToUTF16(
          promo_code_offer->GetDisplayStrings().value_prop_text))}};
    }
    suggestion.payload = Suggestion::BackendId(
        base::NumberToString(promo_code_offer->GetOfferId()));
    suggestion.frontend_id = POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY;

    // Every offer for a given merchant leads to the same GURL, so we grab the
    // first offer's offer details url as the payload for the footer to set
    // later.
    if (footer_offer_details_url.is_empty() &&
        !promo_code_offer->GetOfferDetailsUrl().is_empty() &&
        promo_code_offer->GetOfferDetailsUrl().is_valid()) {
      footer_offer_details_url = promo_code_offer->GetOfferDetailsUrl();
    }
  }

  // Ensure that there are suggestions and that we were able to find at least
  // one suggestion with a valid offer details url before adding the footer.
  DCHECK(suggestions.size() > 0);
  if (!footer_offer_details_url.is_empty()) {
    // Add the footer separator since we will now have a footer in the offers
    // suggestions popup.
    suggestions.emplace_back();
    suggestions.back().frontend_id = POPUP_ITEM_ID_SEPARATOR;

    // Add the footer suggestion that navigates the user to the promo code
    // details page in the offers suggestions popup.
    suggestions.emplace_back(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
    Suggestion& suggestion = suggestions.back();
    suggestion.frontend_id = POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS;

    // We set the payload for the footer as |footer_offer_details_url|, which is
    // the offer details url of the first offer we had for this merchant. We
    // will navigate to the url in |footer_offer_details_url| if the footer is
    // selected in AutofillExternalDelegate::DidAcceptSuggestion().
    suggestion.payload = std::move(footer_offer_details_url);
    suggestion.trailing_icon = "google";
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

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
int AutofillSuggestionGenerator::MakeFrontendId(
    const Suggestion::BackendId& cc_backend_id,
    const Suggestion::BackendId& profile_backend_id) {
  InternalId cc_int_id = BackendIdToInternalId(cc_backend_id);
  InternalId profile_int_id = BackendIdToInternalId(profile_backend_id);

  // Should fit in signed 16-bit integers. We use 16-bits each when combining
  // below, and negative frontend IDs have special meaning so we can never use
  // the high bit.
  DCHECK(cc_int_id.value() <= std::numeric_limits<int16_t>::max());
  DCHECK(profile_int_id.value() <= std::numeric_limits<int16_t>::max());

  // Put CC in the high half of the bits.
  return (cc_int_id.value() << std::numeric_limits<uint16_t>::digits) |
         profile_int_id.value();
}

// When receiving IDs (across processes) from the renderer we unpack credit
// card and profile IDs from a single integer.  Credit card IDs are stored in
// the high word and profile IDs are stored in the low word.
void AutofillSuggestionGenerator::SplitFrontendId(
    int frontend_id,
    Suggestion::BackendId* cc_backend_id,
    Suggestion::BackendId* profile_backend_id) {
  InternalId cc_int_id =
      InternalId((frontend_id >> std::numeric_limits<uint16_t>::digits) &
                 std::numeric_limits<uint16_t>::max());
  InternalId profile_int_id =
      InternalId(frontend_id & std::numeric_limits<uint16_t>::max());

  *cc_backend_id = InternalIdToBackendId(cc_int_id);
  *profile_backend_id = InternalIdToBackendId(profile_int_id);
}

// TODO(crbug.com/1346331): Separate logic for desktop, Android dropdown, and
// Keyboard Accessory.
Suggestion AutofillSuggestionGenerator::CreateCreditCardSuggestion(
    const CreditCard& credit_card,
    const AutofillType& type,
    bool prefix_matched_suggestion,
    bool virtual_card_option,
    const std::string& app_locale,
    bool card_linked_offer_available) const {
  // The kAutofillKeyboardAccessory feature is only available on Android. So for
  // other platforms, we'd always use the obfuscation_length of 4.
  int obfuscation_length =
      base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory) ? 2
                                                                         : 4;

  Suggestion suggestion;
  suggestion.main_text =
      type.GetStorableType() == CREDIT_CARD_NUMBER
          ? Suggestion::Text(credit_card.CardIdentifierStringForAutofillDisplay(
                                 GetDisplayNicknameForCreditCard(credit_card),
                                 obfuscation_length),
                             Suggestion::Text::IsPrimary(true))
          : Suggestion::Text(credit_card.GetInfo(type, app_locale),
                             Suggestion::Text::IsPrimary(true));
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableVirtualCardMetadata) &&
      type.GetStorableType() == CREDIT_CARD_NUMBER) {
    // For the Android dropdown, populate the card name (nickname/product
    // description/network) and the last 4 digits separately to allow them to
    // be shown in separate views. If the suggestion text overflows, only the
    // card name gets truncated in the view.
    suggestion.main_text =
        Suggestion::Text(credit_card.CardNameForAutofillDisplay(
                             GetDisplayNicknameForCreditCard(credit_card)),
                         Suggestion::Text::IsPrimary(true));
    suggestion.minor_text = Suggestion::Text(
        credit_card.ObfuscatedLastFourDigits(obfuscation_length),
        Suggestion::Text::IsPrimary(true));
  }
#endif

  suggestion.icon = credit_card.CardIconStringForAutofillSuggestion();
  suggestion.payload = Suggestion::BackendId(credit_card.guid());
  suggestion.match = prefix_matched_suggestion ? Suggestion::PREFIX_MATCH
                                               : Suggestion::SUBSTRING_MATCH;
#if BUILDFLAG(IS_ANDROID)
  // The card art icon should always be shown at the start of the suggestion.
  suggestion.is_icon_at_start = true;
#endif  // BUILDFLAG(IS_ANDROID)

  std::u16string card_label =
      GetCardLabel(credit_card, type, app_locale, obfuscation_length);
  if (!card_label.empty())
    suggestion.labels = {{Suggestion::Text(std::move(card_label))}};

  // For virtual cards, make some adjustments for the suggestion contents.
  if (virtual_card_option) {
    // We don't show card linked offers for virtual card options.
    AdjustSuggestionContentForVirtualCard(&suggestion, credit_card, type);
  } else if (card_linked_offer_available) {
    // If Keyboard Accessory is not enabled (i.e. Desktop or Clank dropdown),
    // populate an offer label.
    if (!base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory)) {
      suggestion.labels.emplace_back(
          std::vector<Suggestion::Text>{Suggestion::Text(
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK))});

      // Otherwise for Keyboard Accessory, set Suggestion::feature_for_iph and
      // change the suggestion icon only if card linked offers are also enabled.
    } else if (base::FeatureList::IsEnabled(
                   features::kAutofillEnableOffersInClankKeyboardAccessory)) {
#if BUILDFLAG(IS_ANDROID)
      suggestion.feature_for_iph =
          feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature.name;
      suggestion.icon = "offerTag";
#endif
    }
  }

  return suggestion;
}

bool AutofillSuggestionGenerator::ShouldShowVirtualCardOption(
    const CreditCard* candidate_card) const {
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

std::u16string AutofillSuggestionGenerator::GetCardLabel(
    const CreditCard& credit_card,
    const AutofillType& type,
    const std::string& app_locale,
    int obfuscation_length) const {
  // If the focused field is a card number field.
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR),
                               app_locale);
#else
    return credit_card.DescriptiveExpiration(app_locale);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  }

  // If the focused field is not a card number field AND the card number is
  // empty (i.e. local cards added via settings page).
  if (credit_card.number().empty()) {
    DCHECK_EQ(credit_card.record_type(), CreditCard::LOCAL_CARD);

    if (credit_card.HasNonEmptyValidNickname())
      return credit_card.nickname();

    if (type.GetStorableType() != CREDIT_CARD_NAME_FULL) {
      return credit_card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL),
                                 app_locale);
    }
    return std::u16string();
  }

  // If the focused field is not a card number field AND the card number is NOT
  // empty.
#if BUILDFLAG(IS_ANDROID)
  // On Android devices, the label is formatted as
  // "Nickname/Network  ••••1234" when the keyboard accessory experiment
  // is disabled and as "••1234" when it's enabled.
  return base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory)
             ? credit_card.ObfuscatedLastFourDigits(obfuscation_length)
             : credit_card.CardIdentifierStringForAutofillDisplay(
                   GetDisplayNicknameForCreditCard(credit_card));
#elif BUILDFLAG(IS_IOS)
  // E.g. "••••1234"".
  return credit_card.ObfuscatedLastFourDigits();
#else
  // E.g. "Nickname/Network  ••••1234, expires on 01/25".
  return credit_card.CardIdentifierStringAndDescriptiveExpiration(app_locale);
#endif
}

void AutofillSuggestionGenerator::AdjustSuggestionContentForVirtualCard(
    Suggestion* suggestion,
    const CreditCard& credit_card,
    const AutofillType& type) const {
  GURL card_art_url_for_virtual_card_option;
  if (credit_card.record_type() == CreditCard::MASKED_SERVER_CARD) {
    card_art_url_for_virtual_card_option = credit_card.card_art_url();
  } else if (credit_card.record_type() == CreditCard::LOCAL_CARD) {
    const CreditCard* server_duplicate_card =
        GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    card_art_url_for_virtual_card_option =
        server_duplicate_card->card_art_url();
    suggestion->payload = Suggestion::BackendId(server_duplicate_card->guid());
  }

  suggestion->frontend_id = POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY;
  suggestion->feature_for_iph =
      feature_engagement::kIPHAutofillVirtualCardSuggestionFeature.name;

  // TODO(crbug.com/1344629): Update "Virtual card" label for other fields.
  // For virtual cards, prefix "Virtual card" label to field suggestions. For
  // card number field in a dropdown, show the "Virtual card" label below the
  // card number for Metadata experiment.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableVirtualCardMetadata) &&
      type.GetStorableType() == CREDIT_CARD_NUMBER &&
      !base::FeatureList::IsEnabled(features::kAutofillKeyboardAccessory)) {
    suggestion->labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE))}};
  } else {
    suggestion->minor_text.value = suggestion->main_text.value;
    suggestion->main_text.value = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);
  }

#if BUILDFLAG(IS_ANDROID)
  suggestion->custom_icon_url = card_art_url_for_virtual_card_option;
#else
  gfx::Image* image = personal_data_->GetCreditCardArtImageForUrl(
      card_art_url_for_virtual_card_option);
  if (image)
    suggestion->custom_icon = *image;
#endif  // BUILDFLAG(IS_ANDROID)
}

InternalId AutofillSuggestionGenerator::BackendIdToInternalId(
    const Suggestion::BackendId& backend_id) {
  if (!base::IsValidGUID(*backend_id))
    return InternalId();

  InternalId& internal_id = backend_to_internal_map_[backend_id];
  if (!internal_id) {
    internal_id = InternalId(backend_to_internal_map_.size());
    internal_to_backend_map_[internal_id] = backend_id;
  }
  DCHECK_EQ(internal_to_backend_map_.size(), backend_to_internal_map_.size());
  return internal_id;
}

Suggestion::BackendId AutofillSuggestionGenerator::InternalIdToBackendId(
    InternalId internal_id) {
  if (!internal_id)
    return Suggestion::BackendId();

  const auto found = internal_to_backend_map_.find(internal_id);
  if (found == internal_to_backend_map_.end()) {
    NOTREACHED();
    return Suggestion::BackendId();
  }
  return found->second;
}

}  // namespace autofill
