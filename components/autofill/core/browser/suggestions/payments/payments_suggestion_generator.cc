// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;
using SuggestionData = SuggestionGenerator::SuggestionData;

std::pair<SuggestionDataSource, std::vector<SuggestionData>>
FetchCreditCardOrCvcFieldSuggestionDataSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    CreditCardSuggestionSummary& summary) {
  // Early return if CVC suggestions are triggered but the client does not
  // support CVC saving (e.g., for iOS WebView). This avoids unnecessary
  // processing, which would ultimately result in an empty suggestion list
  // anyway.
  if (kCvcFieldTypes.contains(trigger_field_type) &&
      !client.IsCvcSavingSupported()) {
    return {SuggestionDataSource::kCreditCard, {}};
  }

  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      !base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    return {SuggestionDataSource::kCreditCard, {}};
  }

  const bool allow_payment_swapping =
      trigger_field.is_autofilled() && IsPaymentsFieldSwappingEnabled();

  bool suppress_disused_cards =
      SanitizeCreditCardFieldValue(trigger_field.value()).empty();
  bool should_prefix_match = !allow_payment_swapping;
  bool require_non_empty_value_on_trigger_field =
      !allow_payment_swapping || kCvcFieldTypes.contains(trigger_field_type);
  std::vector<CreditCard> cards_to_suggest =
      GetOrderedCardsToSuggest(client, trigger_field, trigger_field_type,
                               /*suppress_disused_cards=*/
                               suppress_disused_cards,
                               /*prefix_match=*/should_prefix_match,
                               /*require_non_empty_value_on_trigger_field=*/
                               require_non_empty_value_on_trigger_field,
                               /*include_virtual_cards=*/true);

  if (kCvcFieldTypes.contains(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFillingEnhancement)) {
    FilterCardsToSuggestForCvcFields(
        trigger_field_type,
        base::flat_set<std::string>(four_digit_combinations_in_dom),
        autofilled_last_four_digits_in_form_for_filtering, cards_to_suggest);
  }

  summary.metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      cards_to_suggest,
      [](auto& card) { return SuggestionData(std::move(card)); });
  return {SuggestionDataSource::kCreditCard, suggestion_data};
}

std::vector<Suggestion> GenerateCreditCardOrCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        suggestion_data) {
  std::vector<Suggestion> suggestions;

  std::map<std::string, const AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(client);
  const std::vector<SuggestionData>* credit_card_data =
      base::FindOrNull(suggestion_data, SuggestionDataSource::kCreditCard);
  if (!credit_card_data) {
    return {};
  }

  std::vector<CreditCard> cards_to_suggest = base::ToVector(
      *credit_card_data, [](const SuggestionData& suggestion_data) {
        return std::get<CreditCard>(suggestion_data);
      });

  for (const CreditCard& credit_card : cards_to_suggest) {
    Suggestion suggestion = CreateCreditCardSuggestion(
        credit_card, client, trigger_field_type,
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard,
        base::Contains(card_linked_offers_map, credit_card.guid()),
        summary.metadata_logging_context);
    suggestions.push_back(suggestion);
  }

  summary.with_cvc = !std::ranges::all_of(
      cards_to_suggest, &std::u16string::empty, &CreditCard::cvc);
  summary.with_card_info_retrieval_enrolled =
      std::ranges::any_of(cards_to_suggest, [](const CreditCard& card) {
        return card.card_info_retrieval_enrollment_state() ==
               CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled;
      });
  if (suggestions.empty()) {
    return suggestions;
  }
  const bool display_gpay_logo = std::ranges::none_of(
      cards_to_suggest,
      [](const CreditCard& card) { return CreditCard::IsLocalCard(&card); });
  const bool should_show_bnpl_suggestion = payments::ShouldAppendBnplSuggestion(
      client, is_card_number_field_empty, trigger_field_type);
  std::ranges::move(
      GetCreditCardFooterSuggestions(
          client, should_show_bnpl_suggestion, should_show_scan_credit_card,
          trigger_field.is_autofilled(), display_gpay_logo),
      std::back_inserter(suggestions));

  return suggestions;
}

std::pair<SuggestionDataSource, std::vector<SuggestionData>>
FetchVirtualCardStandaloneCvcFieldSuggestionDataSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      client, trigger_field, CREDIT_CARD_VERIFICATION_CODE,
      /*suppress_disused_cards=*/true, /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/false);
  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      cards_to_suggest,
      [](auto& card) { return SuggestionData(std::move(card)); });
  return {SuggestionDataSource::kVirtualStandaloneCvc, suggestion_data};
}

std::vector<Suggestion> GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const base::flat_map<std::string,
                         VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        suggestion_data) {
  std::vector<Suggestion> suggestions;

  const std::vector<SuggestionData>* credit_card_data = base::FindOrNull(
      suggestion_data, SuggestionDataSource::kVirtualStandaloneCvc);
  if (!credit_card_data) {
    return {};
  }

  std::vector<CreditCard> cards_to_suggest = base::ToVector(
      *credit_card_data, [](const SuggestionData& suggestion_data) {
        return std::get<CreditCard>(suggestion_data);
      });

  for (const CreditCard& credit_card : cards_to_suggest) {
    auto it = virtual_card_guid_to_last_four_map.find(credit_card.guid());
    if (it == virtual_card_guid_to_last_four_map.end()) {
      continue;
    }
    const std::u16string& virtual_card_last_four = *it->second;

    Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
    suggestion.icon = credit_card.CardIconForAutofillSuggestion();
    suggestion.payload = Suggestion::Guid(credit_card.guid());
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature);
    SetCardArtURL(suggestion, credit_card,
                  client.GetPersonalDataManager().payments_data_manager(),
                  /*virtual_card_option=*/true);
    // TODO(crbug.com/41483863): Create translation string for standalone CVC
    // suggestion which includes spacing.
    const std::u16string main_text =
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_TITLE) +
        u" " +
        CreditCard::GetObfuscatedStringForCardDigits(
            GetCreditCardObfuscationLength(), virtual_card_last_four);
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      // For Android keyboard accessory, we concatenate all the content to the
      // `main_text` to prevent the suggestion descriptor from being cut off.
      suggestion.main_text.value = base::StrCat(
          {main_text, u"  ", credit_card.CardNameForAutofillDisplay()});
    } else {
      suggestion.main_text.value = main_text;
      suggestion.labels = {
          {Suggestion::Text(credit_card.CardNameForAutofillDisplay())}};
    }
    suggestions.push_back(suggestion);
  }

  if (suggestions.empty()) {
    return suggestions;
  }

  std::ranges::move(
      GetCreditCardFooterSuggestions(
          client, /*should_show_bnpl_suggestion=*/false,
          /*should_show_scan_credit_card=*/false, trigger_field.is_autofilled(),
          /*with_gpay_logo=*/true),
      std::back_inserter(suggestions));

  return suggestions;
}

std::vector<Suggestion> GetSuggestionsForCreditCards(
    AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    CreditCardSuggestionSummary& summary,
    bool is_complete_form,
    bool should_show_scan_credit_card,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    bool is_card_number_field_empty) {
  std::vector<Suggestion> suggestions;
  if (base::FeatureList::IsEnabled(features::kAutofillEnableSaveAndFill) &&
      ShouldShowCreditCardSaveAndFill(client, is_complete_form,
                                      trigger_field)) {
    bool display_gpay_logo = false;
    suggestions.push_back(
        CreateSaveAndFillSuggestion(client, display_gpay_logo));
    std::ranges::move(GetCreditCardFooterSuggestions(
                          client, /*should_show_bnpl_suggestion=*/false,
                          should_show_scan_credit_card,
                          trigger_field.is_autofilled(), display_gpay_logo),
                      std::back_inserter(suggestions));
    return suggestions;
  }
  // Only trigger GetVirtualCreditCardsForStandaloneCvcField if it's standalone
  // CVC field.
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
    virtual_card_guid_to_last_four_map =
        GetVirtualCreditCardsForStandaloneCvcField(
            client.GetPersonalDataManager().payments_data_manager(),
            trigger_field.origin(), four_digit_combinations_in_dom);
  }
  // Non-empty virtual_card_guid_to_last_four_map indicates this is standalone
  // CVC form AND there is matched VCN (based on the VCN usages and last four
  // from the DOM).
  if (!virtual_card_guid_to_last_four_map.empty()) {
    // TODO(crbug.com/40916587): Refactor credit card suggestion code by moving
    // duplicate logic to helper functions.
    std::pair<SuggestionDataSource, std::vector<SuggestionData>>
        suggestion_data = FetchVirtualCardStandaloneCvcFieldSuggestionDataSync(
            client, trigger_field, summary.metadata_logging_context);

    suggestions = GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
        client, trigger_field, virtual_card_guid_to_last_four_map,
        {suggestion_data});
  } else {
    // If no virtual cards available for standalone CVC field, fall back to
    // regular credit card suggestions.
    std::pair<SuggestionDataSource, std::vector<SuggestionData>>
        suggestion_data = FetchCreditCardOrCvcFieldSuggestionDataSync(
            client, trigger_field, trigger_field_type,
            four_digit_combinations_in_dom,
            autofilled_last_four_digits_in_form_for_filtering, summary);

    return GenerateCreditCardOrCvcFieldSuggestionsSync(
        client, trigger_field, trigger_field_type, should_show_scan_credit_card,
        summary, is_card_number_field_empty, {suggestion_data});
  }

  return suggestions;
}

}  // namespace autofill
