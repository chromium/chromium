// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"

namespace autofill {

namespace {

Suggestion CreateBnplSuggestion(
    const payments::BnplIssuerContext& issuer_context,
    const std::string& app_locale,
    const bool is_card_number_field_empty) {
  const bool is_linked = issuer_context.issuer.payment_instrument().has_value();

  Suggestion bnpl_suggestion(SuggestionType::kBnplEntry);
  bnpl_suggestion.main_text =
      Suggestion::Text(issuer_context.issuer.GetDisplayName(),
                       Suggestion::Text::IsPrimary(true));
  if (is_card_number_field_empty) {
    bnpl_suggestion.labels = {
        {Suggestion::Text(payments::GetBnplIssuerSelectionOptionText(
            issuer_context.issuer.issuer_id(), app_locale,
            base::span_from_ref(issuer_context)))}};
  } else {
    bnpl_suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_BNPL_PAY_LATER_CLEAR_FORM_TO_ENABLE))}};
    bnpl_suggestion.acceptability =
        Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
  }
  bnpl_suggestion.icon = payments::GetBnplSuggestionIcon(
      issuer_context.issuer.issuer_id(), is_linked);
  bnpl_suggestion.payload = Suggestion::BnplIssuer(issuer_context.issuer);

  return bnpl_suggestion;
}

}  // namespace

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
      // TODO(crbug.com/393114125): Change to use
      // `AutofillField::field_modifiers_` after launching
      // `kAutofillFixIsAutofilled`.
      trigger_field.is_autofilled_according_to_renderer() &&
      IsPaymentsFieldSwappingEnabled();

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
  return {SuggestionDataSource::kCreditCard, std::move(suggestion_data)};
}

std::vector<Suggestion> GenerateCreditCardOrCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        suggestion_data,
    const payments::AmountExtractionStatus& amount_extraction_status) {
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
        card_linked_offers_map.contains(credit_card.guid()),
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

  const bool should_show_pay_later_tab_suggestions =
      payments::ShouldShowBnplSuggestions(client, trigger_field_type) &&
      base::FeatureList::IsEnabled(features::kAutofillEnablePayNowPayLaterTabs);
  const bool should_append_bnpl_suggestion =
      payments::ShouldShowBnplSuggestions(client, trigger_field_type) &&
      is_card_number_field_empty &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs);

  if (should_show_pay_later_tab_suggestions) {
    const PaymentsDataManager& payments_data_manager =
        client.GetPersonalDataManager().payments_data_manager();
    if (payments::ShouldStartPayLaterWithLoadingSpinner(
            payments_data_manager)) {
      Suggestion loading_suggestion =
          Suggestion(SuggestionType::kLoadingThrobber);
      loading_suggestion.acceptability =
          Suggestion::Acceptability::kUnacceptable;
      loading_suggestion.expected_number_of_suggestions =
          payments_data_manager.GetBnplIssuers().size();
      suggestions.push_back(std::move(loading_suggestion));
    } else {
      for (const payments::BnplIssuerContext& context :
           payments::GetSortedBnplIssuerContext(
               client, /*checkout_amount=*/std::nullopt)) {
        suggestions.push_back(CreateBnplSuggestion(
            context, client.GetAppLocale(), is_card_number_field_empty));
      }
    }
  }

  std::ranges::move(
      GetCreditCardFooterSuggestions(
          client, should_show_pay_later_tab_suggestions,
          should_append_bnpl_suggestion, should_show_scan_credit_card,
          // TODO(crbug.com/393114125): Change to use
          // `AutofillField::field_modifiers_` after launching
          // `kAutofillFixIsAutofilled`.
          trigger_field.is_autofilled_according_to_renderer(),
          display_gpay_logo, amount_extraction_status),
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
  return {SuggestionDataSource::kVirtualStandaloneCvc,
          std::move(suggestion_data)};
}

std::vector<Suggestion> GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const base::flat_map<std::string,
                         VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        suggestion_data,
    const payments::AmountExtractionStatus& amount_extraction_status) {
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
    SetCardArtURL(suggestion, credit_card, client,
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

  std::ranges::move(GetCreditCardFooterSuggestions(
                        client, /*should_show_pay_later_tab_suggestions=*/false,
                        /*should_append_bnpl_suggestion=*/false,
                        /*should_show_scan_credit_card=*/false,
                        // TODO(crbug.com/393114125): Change to use
                        // `AutofillField::field_modifiers_` after launching
                        // `kAutofillFixIsAutofilled`.
                        trigger_field.is_autofilled_according_to_renderer(),
                        /*with_gpay_logo=*/true, amount_extraction_status),
                    std::back_inserter(suggestions));

  return suggestions;
}

std::vector<Suggestion> GetSuggestionsForCreditCards(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const AutofillField& autofill_trigger_field,
    AutofillClient& client,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const payments::AmountExtractionStatus& amount_extraction_status,
    autofill_metrics::CreditCardFormEventLogger& credit_card_form_event_logger,
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    bool exclude_virtual_cards) {
  std::vector<Suggestion> suggestions;
  CreditCardSuggestionGenerator credit_card_suggestion_generator(
      four_digit_combinations_in_dom, amount_extraction_status,
      &credit_card_form_event_logger, signin_state_for_metrics,
      exclude_virtual_cards);

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  auto on_suggestion_data_returned =
      [&](std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>>
              suggestion_data) {
        credit_card_suggestion_generator.GenerateSuggestions(
            form, trigger_field, &form_structure, &autofill_trigger_field,
            client, {std::move(suggestion_data)},
            std::move(on_suggestions_generated));
      };

  // Since the `on_suggestions_generated` callback is called synchronously,
  // we can assume that `suggestions` will hold correct value.
  credit_card_suggestion_generator.FetchSuggestionData(
      form, trigger_field, &form_structure, &autofill_trigger_field, client,
      std::move(on_suggestion_data_returned));
  return suggestions;
}

CreditCardSuggestionGenerator::CreditCardSuggestionGenerator(
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const payments::AmountExtractionStatus& amount_extraction_status,
    autofill_metrics::CreditCardFormEventLogger* credit_card_form_event_logger,
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    bool exclude_virtual_cards)
    : four_digit_combinations_in_dom_(four_digit_combinations_in_dom),
      summary_(CreditCardSuggestionSummary()),
      amount_extraction_status_(amount_extraction_status),
      credit_card_form_event_logger_(credit_card_form_event_logger),
      signin_state_for_metrics_(signin_state_for_metrics),
      exclude_virtual_cards_(exclude_virtual_cards) {}

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
  if (credit_card_form_event_logger_) {
    credit_card_form_event_logger_->set_signin_state_for_metrics(
        signin_state_for_metrics_);
  }
  std::u16string card_number_field_value = u"";
  bool is_card_number_autofilled = false;

  // Preprocess the form to extract info about card number field.
  for (const FormFieldData& field : form.fields()) {
    if (const AutofillField* autofill_field =
            form_structure->GetFieldById(field.global_id());
        autofill_field &&
        autofill_field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER) {
      card_number_field_value += SanitizeCreditCardFieldValue(field.value());
      // TODO(crbug.com/393114125): Change to use
      // `AutofillField::field_modifiers_` after launching
      // `kAutofillFixIsAutofilled`.
      is_card_number_autofilled |= field.is_autofilled_according_to_renderer();
    }
  }

  // Offer suggestion for expiration date field if the card number field is
  // empty or the card number field is autofilled.
  auto ShouldOfferSuggestionsForExpirationTypeField = [&] {
    return SanitizedFieldIsEmpty(card_number_field_value) ||
           is_card_number_autofilled;
  };

  if (data_util::IsCreditCardExpirationType(
          trigger_autofill_field->Type().GetCreditCardType()) &&
      !ShouldOfferSuggestionsForExpirationTypeField()) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (IsInAutofillSuggestionsDisabledExperiment()) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  bool is_complete_form = form_structure->IsCompleteCreditCardForm(
      FormStructure::CreditCardFormCompleteness::
          kCompleteCreditCardFormIncludingCvcAndName);

  if (base::FeatureList::IsEnabled(features::kAutofillEnableSaveAndFill) &&
      ShouldShowCreditCardSaveAndFill(const_cast<AutofillClient&>(client),
                                      is_complete_form, trigger_field)) {
    callback({SuggestionDataSource::kSaveAndFillPromo, {}});
    return;
  }

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  if (trigger_autofill_field->Type().GetCreditCardType() ==
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
    virtual_card_guid_to_last_four_map =
        GetVirtualCreditCardsForStandaloneCvcField(
            client.GetPersonalDataManager().payments_data_manager(),
            trigger_field.origin(), four_digit_combinations_in_dom_.get());
  }

  // Non-empty virtual_card_guid_to_last_four_map indicates this is standalone
  // CVC form AND there is matched VCN (based on the VCN usages and last four
  // from the DOM).
  if (!virtual_card_guid_to_last_four_map.empty() && !exclude_virtual_cards_) {
    // TODO(crbug.com/40916587): Refactor credit card suggestion code by moving
    // duplicate logic to helper functions.
    callback(FetchVirtualCardStandaloneCvcFieldSuggestionDataSync(
        client, trigger_field, summary_.metadata_logging_context));
    return;
  }

  const std::u16string autofilled_last_four_digits_in_form_for_filtering =
      is_card_number_autofilled && card_number_field_value.size() >= 4
          ? card_number_field_value.substr(card_number_field_value.size() - 4)
          : u"";
  // If no virtual cards available for standalone CVC field, fall back to
  // regular credit card suggestions.
  callback(FetchCreditCardOrCvcFieldSuggestionDataSync(
      client, trigger_field, trigger_autofill_field->Type().GetCreditCardType(),
      four_digit_combinations_in_dom_.get(),
      autofilled_last_four_digits_in_form_for_filtering, summary_));
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
  if (all_suggestion_data.contains(SuggestionDataSource::kSaveAndFillPromo)) {
    bool display_gpay_logo = false;
    suggestions.push_back(
        CreateSaveAndFillSuggestion(client, display_gpay_logo));
    base::Extend(suggestions,
                 GetCreditCardFooterSuggestions(
                     client, /*should_show_pay_later_tab_suggestions=*/false,
                     /*should_append_bnpl_suggestion=*/false,
                     ShouldShowScanCreditCard(*form_structure,
                                              *trigger_autofill_field, client),
                     // TODO(crbug.com/393114125): Change to use
                     // `AutofillField::field_modifiers_` after launching
                     // `kAutofillFixIsAutofilled`.
                     trigger_field.is_autofilled_according_to_renderer(),
                     display_gpay_logo, amount_extraction_status_.get()));
  } else if (all_suggestion_data.contains(
                 SuggestionDataSource::kVirtualStandaloneCvc)) {
    // Only trigger GetVirtualCreditCardsForStandaloneCvcField if it's
    // standalone CVC field.
    base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
        virtual_card_guid_to_last_four_map;
    if (trigger_autofill_field->Type().GetCreditCardType() ==
        CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
      virtual_card_guid_to_last_four_map =
          GetVirtualCreditCardsForStandaloneCvcField(
              client.GetPersonalDataManager().payments_data_manager(),
              trigger_field.origin(), four_digit_combinations_in_dom_.get());
    }

    suggestions = GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
        client, trigger_field, virtual_card_guid_to_last_four_map,
        all_suggestion_data, amount_extraction_status_.get());
  } else {
    std::u16string card_number_field_value = u"";
    // Preprocess the form to extract info about card number field.
    for (const FormFieldData& field : form.fields()) {
      if (const AutofillField* autofill_field =
              form_structure->GetFieldById(field.global_id());
          autofill_field &&
          autofill_field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER) {
        card_number_field_value += SanitizeCreditCardFieldValue(field.value());
      }
    }

    bool is_card_number_field_empty = card_number_field_value.empty();

    suggestions = GenerateCreditCardOrCvcFieldSuggestionsSync(
        client, trigger_field,
        trigger_autofill_field->Type().GetCreditCardType(),
        ShouldShowScanCreditCard(*form_structure, *trigger_autofill_field,
                                 client),
        summary_, is_card_number_field_empty, all_suggestion_data,
        amount_extraction_status_.get());
  }

  bool is_virtual_card_standalone_cvc_field =
      std::ranges::any_of(suggestions, [](Suggestion suggestion) {
        return suggestion.type == SuggestionType::kVirtualCreditCardEntry;
      });

  if (credit_card_form_event_logger_) {
    credit_card_form_event_logger_->OnDidFetchSuggestion(
        suggestions, summary_.with_cvc,
        summary_.with_card_info_retrieval_enrolled,
        is_virtual_card_standalone_cvc_field,
        std::move(summary_.metadata_logging_context));
  }

  // Don't provide credit card suggestions for non-secure pages, but do provide
  // them for secure pages with passive mixed content (see implementation of
  // IsContextSecure).
  if (!suggestions.empty() &&
      IsFormOrClientNonSecure(client, *form_structure)) {
    // Replace the suggestion content with a warning message explaining why
    // Autofill is disabled for a website. The string is different if the credit
    // card autofill HTTP warning experiment is enabled.
    suggestions = {Suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
        SuggestionType::kInsecureContextPaymentDisabledMessage)};
  }

  callback({FillingProduct::kCreditCard, suggestions});
}

}  // namespace autofill
