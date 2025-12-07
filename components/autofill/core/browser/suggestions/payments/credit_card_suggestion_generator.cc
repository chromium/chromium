// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/save_and_fill_suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
namespace autofill {

using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

constexpr char16_t kEllipsisDotSeparator[] = u"\u2022";

CreditCardSuggestionGenerator::CreditCardSuggestionGenerator(
    payments::SaveAndFillManager* save_and_fill_manager,
    autofill_metrics::CreditCardFormEventLogger* event_logger,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    const std::vector<std::string>& four_digit_combinations_in_dom)
    : save_and_fill_manager_(save_and_fill_manager),
      credit_card_form_event_logger_(*event_logger),
      signin_state_for_metrics_(signin_state_for_metrics),
      four_digit_combinations_in_dom_(four_digit_combinations_in_dom) {}

CreditCardSuggestionGenerator::~CreditCardSuggestionGenerator() = default;

void CreditCardSuggestionGenerator::FetchSuggestionData(
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

void CreditCardSuggestionGenerator::GenerateSuggestions(
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

void CreditCardSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!client.GetPaymentsAutofillClient()->IsAutofillPaymentMethodsEnabled()) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (!form_structure || !trigger_autofill_field) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (trigger_autofill_field->Type().GetTypes().contains_none(
          FieldTypesOfGroup(FieldTypeGroup::kCreditCard)) &&
      trigger_autofill_field->Type().GetTypes().contains_none(
          FieldTypesOfGroup(FieldTypeGroup::kStandaloneCvcField))) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (bool is_complete_form = form_structure->IsCompleteCreditCardForm(
          FormStructure::CreditCardFormCompleteness::
              kCompleteCreditCardFormIncludingCvcAndName);
      base::FeatureList::IsEnabled(features::kAutofillEnableSaveAndFill) &&
      ShouldShowCreditCardSaveAndFill(client, is_complete_form,
                                      trigger_field)) {
    callback({SuggestionDataSource::kSaveAndFillPromo,
              {SaveAndFillSuggestion(true)}});
    return;
  }

  auto [card_number_field_value, is_card_number_autofilled] =
      ExtractInfoFromCardForm(form, form_structure);

  // Offer suggestion for expiration date field if the card number field is
  // empty or the card number field is autofilled.
  if (data_util::IsCreditCardExpirationType(
          trigger_autofill_field->Type().GetCreditCardType()) &&
      !SanitizedFieldIsEmpty(card_number_field_value) &&
      !is_card_number_autofilled) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (IsInAutofillSuggestionsDisabledExperiment()) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  // Only trigger GetVirtualCreditCardsForStandaloneCvcField if it's standalone
  // CVC field.
  if (trigger_autofill_field->Type().GetCreditCardType() ==
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
    virtual_card_guid_to_last_four_map =
        GetVirtualCreditCardsForStandaloneCvcField(client,
                                                   trigger_field.origin());
  }

  // Non-empty virtual_card_guid_to_last_four_map indicates this is a standalone
  // CVC form AND there is a matched VCN (based on the VCN usages and the last
  // four from the DOM).
  if (!virtual_card_guid_to_last_four_map.empty()) {
    std::vector<CreditCard> credit_cards =
        FetchVirtualCardsForStandaloneCvcField(
            client, trigger_field, virtual_card_guid_to_last_four_map);

    std::vector<SuggestionData> suggestion_data = base::ToVector(
        credit_cards,
        [&virtual_card_guid_to_last_four_map](const CreditCard& card) {
          return SuggestionData(std::make_pair(
              card, virtual_card_guid_to_last_four_map[card.guid()].value()));
        });

    callback({SuggestionDataSource::kVirtualStandaloneCvc, suggestion_data});

  } else {
    // If no virtual cards available for standalone CVC field, fall back to
    // regular credit card suggestions.
    std::u16string autofilled_last_four_digits_in_form_for_filtering = u"";

    if (is_card_number_autofilled && card_number_field_value.size() >= 4) {
      autofilled_last_four_digits_in_form_for_filtering =
          card_number_field_value.substr(card_number_field_value.size() - 4);
    }

    std::vector<CreditCard> credit_cards =
        FetchCreditCardsForCreditCardOrCvcField(
            client, trigger_field,
            autofilled_last_four_digits_in_form_for_filtering,
            trigger_autofill_field->Type().GetCreditCardType(),
            ShouldShowScanCreditCard(client, form, trigger_field,
                                     trigger_autofill_field));

    std::vector<SuggestionData> suggestion_data = base::ToVector(
        credit_cards,
        [](auto& card) { return SuggestionData(std::move(card)); });

    callback({SuggestionDataSource::kCreditCard, std::move(suggestion_data)});
  }

  return;
}

void CreditCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  std::vector<Suggestion> suggestions;

  credit_card_form_event_logger_->set_signin_state_for_metrics(
      signin_state_for_metrics_.get());
  CreditCardSuggestionSummary summary;

  bool is_virtual_card_standalone_cvc_field = false;

  // Only one of SuggestionDataSource::[kCreditCard | kSaveAndFillPromo |
  // kVirtualStandaloneCvc] should be present in all_suggestion_data, because of
  // how CCSG::FetchSuggestionData works.
  if (const std::vector<SuggestionData>* credit_card_data = base::FindOrNull(
          all_suggestion_data, SuggestionDataSource::kCreditCard)) {
    std::vector<CreditCard> credit_cards = base::ToVector(
        *credit_card_data, [](const SuggestionData& suggestion_data) {
          return std::get<CreditCard>(suggestion_data);
        });

    auto [card_number_field_value, _] =
        ExtractInfoFromCardForm(form, form_structure);

    suggestions = GenerateSuggestionsForCreditCardOrCvcField(
        trigger_field, trigger_autofill_field->Type().GetCreditCardType(),
        client, credit_cards, summary,
        ShouldShowScanCreditCard(client, form, trigger_field,
                                 trigger_autofill_field),
        card_number_field_value.empty());

  } else if (base::FindOrNull(all_suggestion_data,
                              SuggestionDataSource::kSaveAndFillPromo)) {
    suggestions = GenerateSuggestionsForSaveAndFill(client, form, trigger_field,
                                                    trigger_autofill_field);
  } else if (const std::vector<SuggestionData>* virtual_cvc_data =
                 base::FindOrNull(
                     all_suggestion_data,
                     SuggestionDataSource::kVirtualStandaloneCvc)) {
    is_virtual_card_standalone_cvc_field = true;

    std::vector<VirtualCardSuggestionData> virtual_suggestion_data =
        base::ToVector(
            *virtual_cvc_data, [](const SuggestionData& suggestion_data) {
              return std::get<VirtualCardSuggestionData>(suggestion_data);
            });

    suggestions = GenerateSuggestionsForStandaloneCvcField(
        trigger_field, client, virtual_suggestion_data,
        summary.metadata_logging_context);
  } else {
    callback({FillingProduct::kCreditCard, {}});
    return;
  }

  credit_card_form_event_logger_->OnDidFetchSuggestion(
      suggestions, summary.with_cvc, summary.with_card_info_retrieval_enrolled,
      is_virtual_card_standalone_cvc_field,
      std::move(summary.metadata_logging_context));

  callback({FillingProduct::kCreditCard, suggestions});
  return;
}

bool CreditCardSuggestionGenerator::ShouldShowCreditCardSaveAndFill(
    const AutofillClient& client,
    bool is_complete_form,
    const FormFieldData& trigger_field) {
  if (!save_and_fill_manager_) {
    return false;
  }
  // Verify the user has no credit cards saved.
  if (!client.GetPersonalDataManager()
           .payments_data_manager()
           .GetCreditCards()
           .empty()) {
    save_and_fill_manager_->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::kHasSavedCards);
    return false;
  }

  // Verify that the feature isn't blocked by the strike database. This can
  // happen when the maximum number of strikes is reached or the cooldown
  // period hasn't passed.
  if (save_and_fill_manager_->ShouldBlockFeature()) {
    save_and_fill_manager_->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kBlockedByStrikeDatabase);
    return false;
  }

  // Verify the user is not in incognito mode.
  if (client.IsOffTheRecord()) {
    save_and_fill_manager_->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kUserInIncognito);
    return false;
  }

  // Verify the credit card form is complete for the purposes of "Save and
  // Fill".
  if (!is_complete_form) {
    save_and_fill_manager_->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kIncompleteCreditCardForm);
    return false;
  }

  // Verify a field within the credit card form is clicked and has no more than
  // 3 characters entered.
  if (trigger_field.value().length() > 3) {
    return false;
  }

  return true;
}

bool CreditCardSuggestionGenerator::ShouldShowScanCreditCard(
    const AutofillClient& client,
    const FormData& form,
    const FormFieldData& trigger_field,
    const AutofillField* trigger_autofill_field) {
  const payments::PaymentsAutofillClient& payments_client =
      *client.GetPaymentsAutofillClient();
  if (!payments_client.HasCreditCardScanFeature() ||
      !payments_client.IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  bool is_card_number_field =
      trigger_autofill_field->Type().GetCreditCardType() ==
          CREDIT_CARD_NUMBER &&
      base::ContainsOnlyChars(StripCardNumberSeparators(trigger_field.value()),
                              u"0123456789");

  if (!is_card_number_field) {
    return false;
  }
  if (IsFormOrClientNonSecure(client, form)) {
    return false;
  }

  static const int kShowScanCreditCardMaxValueLength = 6;
  return trigger_field.value().size() <= kShowScanCreditCardMaxValueLength;
}

base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
CreditCardSuggestionGenerator::GetVirtualCreditCardsForStandaloneCvcField(
    const AutofillClient& client,
    const url::Origin& origin) {
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;

  base::span<const VirtualCardUsageData> usage_data =
      client.GetPersonalDataManager()
          .payments_data_manager()
          .GetVirtualCardUsageData();

  for (const CreditCard* card : client.GetPersonalDataManager()
                                    .payments_data_manager()
                                    .GetCreditCards()) {
    // As we only provide virtual card suggestions for standalone CVC fields,
    // check if the card is an enrolled virtual card.
    if (card->virtual_card_enrollment_state() !=
        CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      continue;
    }

    auto matches_card_and_origin = [&](VirtualCardUsageData ud) {
      return ud.instrument_id().value() == card->instrument_id() &&
             ud.merchant_origin() == origin;
    };

    // If `card` has eligible usage data on `origin`, check if the last
    // four digits of `card`'s number occur in the DOM.
    if (auto it = std::ranges::find_if(usage_data, matches_card_and_origin);
        it != usage_data.end()) {
      VirtualCardUsageData::VirtualCardLastFour virtual_card_last_four =
          it->virtual_card_last_four();

      if (base::Contains(four_digit_combinations_in_dom_.get(),
                         base::UTF16ToUTF8(virtual_card_last_four.value()))) {
        // Card has usage data on webpage and last four is present in DOM.
        virtual_card_guid_to_last_four_map[card->guid()] =
            virtual_card_last_four;
      }
    }
  }

  return virtual_card_guid_to_last_four_map;
}

std::vector<CreditCard>
CreditCardSuggestionGenerator::FetchCreditCardsForCreditCardOrCvcField(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card) {
  // Early return if CVC suggestions are triggered but the client does not
  // support CVC saving (e.g., for iOS WebView). This avoids unnecessary
  // processing, which would ultimately result in an empty suggestion list
  // anyway.
  if (kCvcFieldTypes.contains(trigger_field_type) &&
      !client.IsCvcSavingSupported()) {
    return {};
  }

  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      !base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    return {};
  }

  const bool allow_payment_swapping =
      trigger_field.is_autofilled() && IsPaymentsFieldSwappingEnabled();

  std::map<std::string, const AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(client);

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
        trigger_field_type, autofilled_last_four_digits_in_form_for_filtering,
        cards_to_suggest);
  }
  return cards_to_suggest;
}

void CreditCardSuggestionGenerator::FilterCardsToSuggestForCvcFields(
    FieldType trigger_field_type,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    std::vector<CreditCard>& cards_to_suggest) {
  if (trigger_field_type ==
          FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    // For standalone CVC fields, there is no form to fill and thus filter based
    // on, so the filtering mechanism used to show the correct suggestion(s) is
    // matching the last four digits in the DOM to the last four digits of the
    // cards that can be displayed.
    std::erase_if(cards_to_suggest, [this](const CreditCard& credit_card) {
      return !base::Contains(this->four_digit_combinations_in_dom_.get(),
                             base::UTF16ToUTF8(credit_card.LastFourDigits()));
    });
  } else {
    // `autofilled_last_four_digits_in_form_for_filtering` being empty implies
    // no card was autofilled, show all suggestions.
    if (autofilled_last_four_digits_in_form_for_filtering.empty()) {
      return;
    }
    std::erase_if(cards_to_suggest,
                  [&autofilled_last_four_digits_in_form_for_filtering](
                      const CreditCard& credit_card) {
                    return autofilled_last_four_digits_in_form_for_filtering !=
                           credit_card.LastFourDigits();
                  });
  }
}

std::vector<CreditCard>
CreditCardSuggestionGenerator::FetchVirtualCardsForStandaloneCvcField(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map) {
  std::vector<CreditCard> credit_cards = GetOrderedCardsToSuggest(
      client, trigger_field, CREDIT_CARD_VERIFICATION_CODE,
      /*suppress_disused_cards=*/true, /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/false);

  // From the general list of cards, remove any that are not a virtual card
  // relevant to the current standalone CVC field.
  std::erase_if(credit_cards, [&](const CreditCard& credit_card) {
    return !virtual_card_guid_to_last_four_map.contains(credit_card.guid());
  });

  return credit_cards;
}

std::vector<Suggestion>
CreditCardSuggestionGenerator::GenerateSuggestionsForCreditCardOrCvcField(
    const FormFieldData& trigger_field,
    const FieldType& trigger_field_type,
    const AutofillClient& client,
    const std::vector<CreditCard>& credit_cards,
    CreditCardSuggestionSummary& summary,
    bool should_show_scan_credit_card,
    bool is_card_number_field_empty) {
  std::map<std::string, const AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(client);

  std::vector<Suggestion> suggestions;

  summary.metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(credit_cards);

  for (const CreditCard& credit_card : credit_cards) {
    Suggestion suggestion = CreateCreditCardSuggestion(
        credit_card, client, trigger_field_type,
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard,
        base::Contains(card_linked_offers_map, credit_card.guid()),
        summary.metadata_logging_context);
    suggestions.push_back(suggestion);
  }

  summary.with_cvc = !std::ranges::all_of(credit_cards, &std::u16string::empty,
                                          &CreditCard::cvc);
  summary.with_card_info_retrieval_enrolled =
      std::ranges::any_of(credit_cards, [](const CreditCard& card) {
        return card.card_info_retrieval_enrollment_state() ==
               CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled;
      });

  if (suggestions.empty()) {
    return suggestions;
  }

  const bool display_gpay_logo = std::ranges::none_of(
      credit_cards,
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

void CreditCardSuggestionGenerator::SetSuggestionLabelsForCard(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type,
    Suggestion& suggestion,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  const std::string& app_locale =
      client.GetPersonalDataManager().payments_data_manager().app_locale();

  if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    suggestion.labels = {{Suggestion::Text(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
        u" • " + credit_card.GetInfo(CREDIT_CARD_TYPE, app_locale) + u" " +
        credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
            GetObfuscationLength()))}};
    return;
  }

  // If the focused field is a card number field.
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    suggestion.labels = {{Suggestion::Text(
        credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))}};
#else
    std::vector<std::vector<Suggestion::Text>> labels;

    // If the main text is the card's nickname or product description,
    // the network, last four digits, and expiration date must be displayed
    // separately in another row.
    if (ShouldUseNewFopDisplay() && !suggestion.main_text.value.empty() &&
        suggestion.minor_texts.empty()) {
      labels.push_back({Suggestion::Text(credit_card.NetworkAndLastFourDigits(
                            GetObfuscationLength())),
                        Suggestion::Text(kEllipsisDotSeparator),
                        Suggestion::Text(credit_card.GetInfo(
                            CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))});
    }
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, client);
    if (benefit_label) {
      // Keep track of which cards had eligible benefits even if the
      // benefit is not displayed in the suggestion due to
      // IsCardEligibleForBenefits() == false. This is to denote a control group
      // of users with benefit-eligible cards and assess how actually
      // displaying the benefit in the experiment influences the users autofill
      // interactions.
      metadata_logging_context.instrument_ids_to_available_benefit_sources
          .insert({credit_card.instrument_id(), credit_card.benefit_source()});
      if (client.GetPersonalDataManager()
              .payments_data_manager()
              .IsCardEligibleForBenefits(credit_card)) {
        labels.push_back({*benefit_label});
        if (base::FeatureList::IsEnabled(
                features::kAutofillEnableCardBenefitsIph)) {
          suggestion.iph_metadata = Suggestion::IPHMetadata(
              &feature_engagement::kIPHAutofillCreditCardBenefitFeature);
        }
      }
    }
    if (!ShouldUseNewFopDisplay()) {
      labels.push_back({Suggestion::Text(
          ShouldSplitCardNameAndLastFourDigits()
              ? credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                    app_locale)
              : credit_card.DescriptiveExpiration(app_locale))});
    }
    suggestion.labels = std::move(labels);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return;
  }

  // If the focused field is not a card number field AND the card number is
  // empty (i.e. local cards added via settings page).
  std::u16string nickname = GetDisplayNicknameForCreditCard(
      credit_card, client.GetPersonalDataManager().payments_data_manager());
  if (credit_card.number().empty()) {
    DCHECK_EQ(credit_card.record_type(), CreditCard::RecordType::kLocalCard);

    if (credit_card.HasNonEmptyValidNickname()) {
      suggestion.labels = {{Suggestion::Text(nickname)}};
    } else if (trigger_field_type != CREDIT_CARD_NAME_FULL) {
      suggestion.labels = {{Suggestion::Text(
          credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale))}};
    }
    return;
  }

  // If the focused field is not a card number field AND the card number is NOT
  // empty.

  if constexpr (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)) {
    if (client.ShouldFormatForLargeKeyboardAccessory()) {
      suggestion.labels = {
          {Suggestion::Text(credit_card.CardNameAndLastFourDigits(
              nickname, GetObfuscationLength()))}};
    } else {
      // On Mobile, the label is formatted as "••1234".
      suggestion.labels = {{Suggestion::Text(
          credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
              GetObfuscationLength()))}};
    }
    return;
  }

  if (ShouldUseNewFopDisplay()) {
    suggestion.labels = {{Suggestion::Text(credit_card.NetworkAndLastFourDigits(
                              GetObfuscationLength())),
                          Suggestion::Text(kEllipsisDotSeparator),
                          Suggestion::Text(credit_card.GetInfo(
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))}};
    return;
  }

  if (ShouldSplitCardNameAndLastFourDigits()) {
    // Format the label as "Product Description/Nickname/Network  ••••1234".
    // If the card name is too long, it will be truncated from the tail.
    suggestion.labels = {
        {Suggestion::Text(credit_card.CardNameForAutofillDisplay(nickname),
                          Suggestion::Text::IsPrimary(false),
                          Suggestion::Text::ShouldTruncate(true)),
         Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
             GetObfuscationLength()))}};
    return;
  }

  // Format the label as
  // "Product Description/Nickname/Network  ••••1234, expires on 01/25".
  suggestion.labels = {{Suggestion::Text(
      credit_card.CardIdentifierStringAndDescriptiveExpiration(app_locale))}};
  return;
}

std::pair<std::u16string, bool>
CreditCardSuggestionGenerator::ExtractInfoFromCardForm(
    const FormData& form,
    const FormStructure* form_structure) {
  std::u16string card_number_field_value = u"";
  bool is_card_number_autofilled = false;

  // Preprocess the form to extract info about card number field.
  for (const FormFieldData& local_field : form.fields()) {
    if (const AutofillField* autofill_field =
            form_structure->GetFieldById(local_field.global_id());
        autofill_field &&
        autofill_field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER) {
      card_number_field_value +=
          SanitizeCreditCardFieldValue(local_field.value());
      is_card_number_autofilled |= local_field.is_autofilled();
    }
  }

  return {card_number_field_value, is_card_number_autofilled};
}

Suggestion CreditCardSuggestionGenerator::CreateCreditCardSuggestion(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  Suggestion suggestion(SuggestionType::kCreditCardEntry);
  suggestion.icon = credit_card.CardIconForAutofillSuggestion();
  suggestion.acceptability = IsCardSuggestionAcceptable(credit_card, client)
                                 ? Suggestion::Acceptability::kAcceptable
                                 : Suggestion::Acceptability::kUnacceptable;
  suggestion.payload = Suggestion::Guid(credit_card.guid());

  // Manual fallback suggestions labels are computed as if the triggering field
  // type was the credit card number.
  auto [main_text, minor_text] = GetSuggestionMainTextAndMinorTextForCard(
      credit_card, client, trigger_field_type);
  suggestion.main_text = std::move(main_text);
  if (!minor_text.value.empty()) {
    if (ShouldUseNewFopDisplay()) {
      suggestion.minor_texts.emplace_back(kEllipsisDotSeparator,
                                          Suggestion::Text::IsPrimary(true));
    }
    suggestion.minor_texts.emplace_back(std::move(minor_text));
  }
  SetSuggestionLabelsForCard(credit_card, client, trigger_field_type,
                             suggestion, metadata_logging_context);
  SetCardArtURL(suggestion, credit_card,
                client.GetPersonalDataManager().payments_data_manager(),
                virtual_card_option);

  // For server card, show card info retrieval enrolled suggestion for card info
  // retrieval enrolled card.
  if (credit_card.card_info_retrieval_enrollment_state() ==
      CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled) {
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillCardInfoRetrievalSuggestionFeature);
    suggestion.iph_metadata.iph_params = {
        GetDisplayNameForIssuerId(credit_card.issuer_id())};
#if BUILDFLAG(IS_ANDROID)
    suggestion.iph_description_text =
        CreateCardInfoRetrievalIphDescriptionText(suggestion);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // For virtual cards, make some adjustments for the suggestion contents.
  if (virtual_card_option) {
    // We don't show card linked offers for virtual card options.
    AdjustVirtualCardSuggestionContent(suggestion, credit_card, client,
                                       trigger_field_type);
  } else if (card_linked_offer_available) {
#if BUILDFLAG(IS_ANDROID)
    // For Keyboard Accessory, set Suggestion::iph_metadata and change the
    // suggestion icon only if card linked offers are also enabled.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableOffersInClankKeyboardAccessory)) {
      suggestion.iph_metadata = Suggestion::IPHMetadata(
          &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature);
      suggestion.icon = Suggestion::Icon::kOfferTag;
    } else {
#else   // Add the offer label on Desktop unconditionally.
    {
#endif  // BUILDFLAG(IS_ANDROID)
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK))});
    }
  }

  if (virtual_card_option) {
    suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_CARD_INFORMATION_ENTRY);
  } else {
    suggestion.acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
  }

  return suggestion;
}

Suggestion CreditCardSuggestionGenerator::CreateSaveAndFillSuggestion(
    const AutofillClient& client,
    bool& display_gpay_logo) {
  Suggestion save_and_fill(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_SUGGESTION_TITLE),
      SuggestionType::kSaveAndFillCreditCardEntry);
  if (client.IsCreditCardUploadEnabled()) {
    save_and_fill.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SERVER_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}};
    display_gpay_logo = true;
  } else {
    save_and_fill.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_LOCAL_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}};
  }
  save_and_fill.icon = Suggestion::Icon::kSaveAndFill;
  return save_and_fill;
}

void CreditCardSuggestionGenerator::AdjustVirtualCardSuggestionContent(
    Suggestion& suggestion,
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type) {
  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        client.GetPersonalDataManager()
            .payments_data_manager()
            .GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    suggestion.payload = Suggestion::Guid(server_duplicate_card->guid());
  }

  suggestion.type = SuggestionType::kVirtualCreditCardEntry;
  // If a virtual card is non-acceptable, it needs to be displayed in
  // grayed-out style.
  if (!suggestion.IsAcceptable()) {
    suggestion.acceptability =
        Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
  }
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      suggestion.HasDeactivatedStyle()
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);

  // If ShouldFormatForLargeKeyboardAccessory() is true, `suggestion` has been
  // properly formatted by `SetSuggestionLabelsForCard` and does not need
  // further changes.
  if (client.ShouldFormatForLargeKeyboardAccessory()) {
    return;
  }

  // Add virtual card labelling to suggestions. For keyboard accessory, it is
  // prefixed to the suggestion, and for the dropdown, it is shown as a label on
  // a separate line.
  const std::u16string& virtual_card_label = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);
  const std::u16string& virtual_card_disabled_label = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE);
#if BUILDFLAG(IS_IOS)
  suggestion.minor_texts = {};
  suggestion.minor_texts.emplace_back(suggestion.main_text.value);
  if (suggestion.IsAcceptable()) {
    suggestion.main_text.value = virtual_card_label;
  } else {
    suggestion.main_text.value = virtual_card_disabled_label;
  }

#elif BUILDFLAG(IS_ANDROID)
  // The keyboard accessory chips can only accommodate 2 strings which are
  // displayed on a single row. The minor_text and the labels are
  // concatenated, so we have: String 1 = main_text, String 2 = minor_text +
  // labels.
  // There is a limit on the size of the keyboard accessory chips. When the
  // suggestion content exceeds this limit, the card name or the cardholder
  // name can be truncated, the last 4 digits should never be truncated.
  // Contents in the main_text are automatically truncated from the right end
  // on the Android side when the size limit is exceeded, so the card name and
  // the cardholder name is appended to the main_text.
  // Here we modify the `Suggestion` members to make it suitable for showing
  // on the keyboard accessory.
  // Card number field:
  // Before: main_text = card name, minor_text = last 4 digits, labels =
  // expiration date.
  // After: main_text = virtual card label + card name, minor_text = last 4
  // digits, labels = null.
  // Cardholder name field:
  // Before: main_text = cardholder name, minor_text = null, labels = last 4
  // digits.
  // After: main_text = virtual card label + cardholder name, minor_text is
  // empty, labels = last 4 digits.
  if (ShouldSplitCardNameAndLastFourDigits()) {
    suggestion.main_text.value =
        base::StrCat({virtual_card_label, u"  ", suggestion.main_text.value});
  } else {
    suggestion.minor_texts = {};
    suggestion.minor_texts.emplace_back(suggestion.main_text.value);
    suggestion.main_text.value = virtual_card_label;
  }
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    // The expiration date is not shown for the card number field, so it is
    // removed.
    suggestion.labels = {};
  }
#else   // Desktop dropdown.
  // The label fields will be consistent regardless of focused field.
  if (ShouldUseNewFopDisplay() || trigger_field_type == CREDIT_CARD_NUMBER) {
    // Reset the labels as we only show benefit and virtual card label to
    // conserve space.
    suggestion.labels = {};
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, client);
    if (ShouldUseNewFopDisplay() && suggestion.minor_texts.empty()) {
      // minor_texts empty means that the card has either nickname or
      // product description, so add network and last four digits as a
      // separate label.
      suggestion.labels = {{Suggestion::Text(
          credit_card.NetworkAndLastFourDigits(GetObfuscationLength()))}};
    }
    if (benefit_label && client.GetPersonalDataManager()
                             .payments_data_manager()
                             .IsCardEligibleForBenefits(credit_card)) {
      // For the new-FOP display feature, when the merchant opts out
      // (that is, the suggestion is not acceptable), the benefit is not
      // shown because a merchant opt-out message will be displayed instead.
      if (!ShouldUseNewFopDisplay() || suggestion.IsAcceptable()) {
        suggestion.labels.push_back({*benefit_label});
      }
    }
  }
  // For the new-FOP display feature, a virtual card label will not be added
  // as it will be shown as a badge.
  if (!ShouldUseNewFopDisplay() && suggestion.IsAcceptable()) {
    suggestion.labels.push_back(
        std::vector<Suggestion::Text>{Suggestion::Text(virtual_card_label)});
  }
  if (!suggestion.IsAcceptable()) {
    suggestion.labels.push_back(std::vector<Suggestion::Text>{
        Suggestion::Text(virtual_card_disabled_label)});
  }
#endif  // BUILDFLAG(IS_IOS)
}

// Returns display name based on `issuer_id` in a vector.
std::u16string CreditCardSuggestionGenerator::GetDisplayNameForIssuerId(
    const std::string& issuer_id) {
  if (issuer_id == "paypay") {
    return u"PayPay";
  }
  return u"";
}

bool CreditCardSuggestionGenerator::IsCreditCardExpiryData(
    FieldType trigger_field_type) {
  switch (trigger_field_type) {
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return true;
    default:
      return false;
  }
}

std::pair<Suggestion::Text, Suggestion::Text>
CreditCardSuggestionGenerator::GetSuggestionMainTextAndMinorTextForCard(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type) {
  if (IsCreditCardExpiryData(trigger_field_type) &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    // For large keyboard accessories, always show the full date regardless of
    // which expiry data related field triggered the suggestion.
    trigger_field_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
  }

  auto create_text =
      [this](std::u16string main_text,
             std::u16string minor_text =
                 u"") -> std::pair<Suggestion::Text, Suggestion::Text> {
    return {Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true),
                             Suggestion::Text::ShouldTruncate(
                                 ShouldSplitCardNameAndLastFourDigits())),
            // minor_text should also be shown in primary style, since it is
            // also on the first line.
            Suggestion::Text(minor_text, Suggestion::Text::IsPrimary(true))};
  };

  std::u16string nickname = GetDisplayNicknameForCreditCard(
      credit_card, client.GetPersonalDataManager().payments_data_manager());
  if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    return create_text(credit_card.CardNameForAutofillDisplay(nickname));
  }

  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    if (ShouldUseNewFopDisplay()) {
      std::optional<std::u16string> identifier =
          credit_card.CardIdentifierForAutofillDisplay(nickname);
      if (identifier.has_value()) {
        return create_text(*identifier);
      } else {
        return create_text(
            credit_card.NetworkAndLastFourDigits(GetObfuscationLength()),
            credit_card.AbbreviatedExpirationDateForDisplay(false));
      }
    }
    if (ShouldSplitCardNameAndLastFourDigits()) {
      return create_text(credit_card.CardNameForAutofillDisplay(nickname),
                         credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
                             GetObfuscationLength()));
    }
    return create_text(credit_card.CardNameAndLastFourDigits(
        nickname, GetObfuscationLength()));
  }

  if (kCvcFieldTypes.contains(trigger_field_type)) {
    CHECK(!credit_card.cvc().empty());
#if BUILDFLAG(IS_ANDROID)
    return create_text(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT,
        credit_card.CardNameForAutofillDisplay(GetDisplayNicknameForCreditCard(
            credit_card,
            client.GetPersonalDataManager().payments_data_manager()))));
#else
    return create_text(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
#endif
  }

  return create_text(credit_card.GetInfo(
      trigger_field_type,
      client.GetPersonalDataManager().payments_data_manager().app_locale()));
}

std::u16string
CreditCardSuggestionGenerator::CreateCardInfoRetrievalIphDescriptionText(
    Suggestion suggestion) {
#if BUILDFLAG(IS_ANDROID)
  std::u16string description_text;
  if (!suggestion.iph_metadata.iph_params.empty() &&
      !suggestion.iph_metadata.iph_params.front().empty()) {
    description_text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SUGGESTION_IPH_BUBBLE_LABEL,
        suggestion.iph_metadata.iph_params.front());
  } else {
    description_text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SUGGESTION_IPH_BUBBLE_FALLBACK_LABEL);
  }

  return description_text;
#else
  return std::u16string();
#endif  // BUILDFLAG(IS_ANDROID)
}

bool CreditCardSuggestionGenerator::ShouldSplitCardNameAndLastFourDigits() {
  return !BUILDFLAG(IS_IOS);
}

std::vector<Suggestion>
CreditCardSuggestionGenerator::GenerateSuggestionsForSaveAndFill(
    const AutofillClient& client,
    const FormData& form,
    const FormFieldData& trigger_field,
    const AutofillField* trigger_autofill_field) {
  std::vector<Suggestion> suggestions;

  bool display_gpay_logo = false;

  suggestions.push_back(CreateSaveAndFillSuggestion(client, display_gpay_logo));

  std::ranges::move(GetCreditCardFooterSuggestions(
                        client,
                        /*should_show_bnpl_suggestion=*/false,
                        ShouldShowScanCreditCard(client, form, trigger_field,
                                                 trigger_autofill_field),
                        trigger_field.is_autofilled(), display_gpay_logo),
                    std::back_inserter(suggestions));

  return suggestions;
}

std::vector<Suggestion>
CreditCardSuggestionGenerator::GenerateSuggestionsForStandaloneCvcField(
    const FormFieldData& trigger_field,
    const AutofillClient& client,
    const std::vector<VirtualCardSuggestionData>& suggestion_data,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  std::vector<Suggestion> suggestions;

  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(base::ToVector(
          suggestion_data, &std::pair<CreditCard, std::u16string>::first));

  for (const auto& [credit_card, virtual_card_last_four] : suggestion_data) {
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
        CreditCard::GetObfuscatedStringForCardDigits(GetObfuscationLength(),
                                                     virtual_card_last_four);
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
      GetCreditCardFooterSuggestions(client,
                                     /*should_show_bnpl_suggestion=*/false,
                                     /*should_show_scan_credit_card=*/false,
                                     trigger_field.is_autofilled(),
                                     /*with_gpay_logo=*/true),
      std::back_inserter(suggestions));

  return suggestions;
}

std::vector<Suggestion>
CreditCardSuggestionGenerator::GetCreditCardFooterSuggestions(
    const AutofillClient& client,
    bool should_show_bnpl_suggestion,
    bool should_show_scan_credit_card,
    bool is_autofilled,
    bool with_gpay_logo) {
  std::vector<Suggestion> footer_suggestions;

  if (should_show_bnpl_suggestion) {
    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString)) {
      footer_suggestions.emplace_back(SuggestionType::kSeparator);
    }

    footer_suggestions.push_back(
        CreateBnplSuggestion(client.GetPersonalDataManager()
                                 .payments_data_manager()
                                 .GetBnplIssuers(),
                             /*extracted_amount_in_micros=*/std::nullopt));
  }

  if (should_show_scan_credit_card) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD),
        SuggestionType::kScanCreditCard);
    scan_credit_card.icon = Suggestion::Icon::kScanCreditCard;
    footer_suggestions.push_back(scan_credit_card);
  }
  footer_suggestions.emplace_back(SuggestionType::kSeparator);
  if (is_autofilled) {
    footer_suggestions.push_back(CreateUndoOrClearFormSuggestion());
  }
  footer_suggestions.push_back(
      CreateManageCreditCardsSuggestion(with_gpay_logo));
  return footer_suggestions;
}

Suggestion CreditCardSuggestionGenerator::CreateUndoOrClearFormSuggestion() {
#if BUILDFLAG(IS_IOS)
  std::u16string value =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
  // TODO(crbug.com/40266549): iOS still uses Clear Form logic, replace with
  // Undo.
  Suggestion suggestion(value, SuggestionType::kUndoOrClear);
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

int CreditCardSuggestionGenerator::GetObfuscationLength() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On Android and iOS, the obfuscation length is 2.
  return 2;
#else
  return ShouldUseNewFopDisplay() ? 2 : 4;
#endif
}

}  // namespace autofill
