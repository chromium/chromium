// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

bool IsSaveAndFillEnabled() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(features::kAutofillEnableSaveAndFill);
#elif BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  return false;
#endif
}

// Fetches a list of `CreditCard`s, used for credit card or cvc field suggestion
// generation. Fetched data will be used in
// GenerateCreditCardOrCvcFieldSuggestionsSync.
std::vector<CreditCard> FetchCreditCardOrCvcFieldSuggestionDataSync(
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
    return {};
  }

  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      !base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    return {};
  }

  const bool allow_payment_swapping =
      // TODO(crbug.com/393114125): Change to use
      // `AutofillField::field_modifiers_`.
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

  if (kCvcFieldTypes.contains(trigger_field_type)) {
    FilterCardsToSuggestForCvcFields(
        trigger_field_type,
        base::flat_set<std::string>(four_digit_combinations_in_dom),
        autofilled_last_four_digits_in_form_for_filtering, cards_to_suggest);
  }

  summary.metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  return cards_to_suggest;
}

// Generates suggestions for all available credit cards based on the
// `trigger_field_type` and `trigger_field`. `summary` contains metadata about
// the returned suggestions. `last_four_set_for_cvc_suggestion_filtering` is a
// set of card number last four that will be used for suggestion filtering. this
// is used to avoid showing suggestions that is unrelated to the cards that have
// already been autofilled in the form.
// `is_card_number_field_empty` indicates whether the card number field is empty
// after the value inside of it is sanitized. this is used to decide whether the
// bnpl suggestion should be appended together with the credit card suggestions.
// TODO(crbug.com/40916587): implement last four extraction from the dom.
// TODO(crbug.com/448688721): Consolidate the input parameters.
std::vector<Suggestion> GenerateCreditCardOrCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty,
    const std::vector<CreditCard>& cards_to_suggest,
    const payments::AmountExtractionStatus& amount_extraction_status,
    payments::BnplManager* bnpl_manager) {
  if (cards_to_suggest.empty()) {
    return {};
  }

  std::vector<Suggestion> suggestions;

  for (const CreditCard& credit_card : cards_to_suggest) {
    Suggestion suggestion = CreateCreditCardSuggestion(
        credit_card, client, trigger_field_type,
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard,
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
  summary.with_externally_saved_card =
      std::ranges::any_of(cards_to_suggest, [](const CreditCard& card) {
        return card.card_creation_source() ==
               CreditCard::CardCreationSource::kCreationSourceNonChromePayments;
      });
  summary.with_never_used_card =
      std::ranges::any_of(cards_to_suggest, [](const CreditCard& card) {
        return card.usage_history().use_count() == 1;
      });

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

  if (should_show_pay_later_tab_suggestions && bnpl_manager) {
    summary.with_pay_later_tab_suggestion = true;
    suggestions.append_range(
        bnpl_manager->GetBnplSuggestions(is_card_number_field_empty));
  }

  base::Extend(suggestions,
               GetCreditCardFooterSuggestions(
                   client, should_show_pay_later_tab_suggestions,
                   should_append_bnpl_suggestion, should_show_scan_credit_card,
                   // TODO(crbug.com/393114125): Change to use
                   // `AutofillField::field_modifiers_`.
                   trigger_field.is_autofilled_according_to_renderer(),
                   display_gpay_logo, amount_extraction_status, bnpl_manager));

  return suggestions;
}

// Fetches a list of `CreditCard`s, used for standalone CVC fields suggestion
// generation. Fetched data will be used in
// GenerateVirtualCardStandaloneCvcFieldSuggestionsSync.
std::vector<CreditCard> FetchVirtualCardStandaloneCvcFieldSuggestionDataSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const base::flat_map<std::string,
                         VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      client, trigger_field, CREDIT_CARD_VERIFICATION_CODE,
      /*suppress_disused_cards=*/true, /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/false);

  // Filter `cards_to_suggest` to only the masked server cards that have a
  // virtual card, where the card's last four digits were seen in the DOM.
  std::erase_if(cards_to_suggest, [&](const CreditCard& card) {
    return !virtual_card_guid_to_last_four_map.contains(card.guid());
  });

  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);
  return cards_to_suggest;
}

// Generates suggestions for standalone CVC fields. These only apply to
// virtual cards that are saved on file to a merchant. In these cases,
// we only display the virtual card option and do not show FPAN option.
std::vector<Suggestion> GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const base::flat_map<std::string,
                         VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map,
    const std::vector<CreditCard>& cards_to_suggest,
    const payments::AmountExtractionStatus& amount_extraction_status) {
  if (cards_to_suggest.empty()) {
    return {};
  }
  std::vector<Suggestion> suggestions;

  for (const CreditCard& credit_card : cards_to_suggest) {
    // The call to `at()` is safe here because in `FetchSuggestionData()` all
    // cards that did not have an entry in this map were removed.
    const std::u16string& virtual_card_last_four =
        *virtual_card_guid_to_last_four_map.at(credit_card.guid());

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

  std::ranges::move(GetCreditCardFooterSuggestions(
                        client, /*should_show_pay_later_tab_suggestions=*/false,
                        /*should_append_bnpl_suggestion=*/false,
                        /*should_show_scan_credit_card=*/false,
                        // TODO(crbug.com/393114125): Change to use
                        // `AutofillField::field_modifiers_`.
                        trigger_field.is_autofilled_according_to_renderer(),
                        /*with_gpay_logo=*/true, amount_extraction_status,
                        /*bnpl_manager=*/nullptr),
                    std::back_inserter(suggestions));

  return suggestions;
}

}  // namespace

std::vector<Suggestion> GetSuggestionsForCreditCards(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const AutofillField& autofill_trigger_field,
    AutofillClient& client,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    payments::AmountExtractionManager* amount_extraction_manager,
    payments::BnplManager* bnpl_manager,
    autofill_metrics::CreditCardFormEventLogger& credit_card_form_event_logger,
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    bool exclude_virtual_cards) {
  std::vector<Suggestion> suggestions;
  CreditCardSuggestionGenerator credit_card_suggestion_generator(
      four_digit_combinations_in_dom, amount_extraction_manager, bnpl_manager,
      &credit_card_form_event_logger, signin_state_for_metrics,
      exclude_virtual_cards);

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  // Since the `on_suggestions_generated` callback is called synchronously,
  // we can assume that `suggestions` will hold the correct value.
  credit_card_suggestion_generator.GenerateSuggestions(
      form, trigger_field, &form_structure, &autofill_trigger_field, client,
      on_suggestions_generated);

  return suggestions;
}

std::vector<Suggestion> GetSuggestionsForBnpl(
    std::vector<payments::BnplIssuerContext> issuer_contexts,
    const std::string& app_locale,
    const bool is_card_number_field_empty) {
  std::vector<Suggestion> bnpl_suggestions;
  bnpl_suggestions.reserve(issuer_contexts.size());

  for (payments::BnplIssuerContext& issuer_context : issuer_contexts) {
    Suggestion bnpl_suggestion(SuggestionType::kBnplEntry);
    bnpl_suggestion.main_text =
        Suggestion::Text(issuer_context.issuer.GetDisplayName(),
                         Suggestion::Text::IsPrimary(true));
    if (is_card_number_field_empty) {
      bnpl_suggestion.acceptability =
          issuer_context.IsEligible()
              ? Suggestion::Acceptability::kAcceptable
              : Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
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
    bnpl_suggestion.icon =
        payments::GetBnplSuggestionIcon(issuer_context.issuer.issuer_id());
    bnpl_suggestion.payload =
        Suggestion::BnplIssuer(std::move(issuer_context.issuer));
    bnpl_suggestion.tab_index = kPayLaterSuggestionTabIndex;
    bnpl_suggestions.emplace_back(bnpl_suggestion);
  }

  return bnpl_suggestions;
}

Suggestion GetLoadingSuggestionForPayLaterTab(
    int expected_number_of_suggestions) {
  Suggestion loading_suggestion = Suggestion(SuggestionType::kLoadingThrobber);
  loading_suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  loading_suggestion.expected_number_of_suggestions =
      expected_number_of_suggestions;
  loading_suggestion.tab_index = kPayLaterSuggestionTabIndex;
  return loading_suggestion;
}

CreditCardSuggestionGenerator::CreditCardSuggestionGenerator(
    const std::vector<std::string>& four_digit_combinations_in_dom,
    payments::AmountExtractionManager* amount_extraction_manager,
    payments::BnplManager* bnpl_manager,
    autofill_metrics::CreditCardFormEventLogger* credit_card_form_event_logger,
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    bool exclude_virtual_cards)
    : four_digit_combinations_in_dom_(four_digit_combinations_in_dom),
      amount_extraction_manager_(amount_extraction_manager),
      bnpl_manager_(bnpl_manager),
      credit_card_form_event_logger_(credit_card_form_event_logger),
      signin_state_for_metrics_(signin_state_for_metrics),
      exclude_virtual_cards_(exclude_virtual_cards) {}

CreditCardSuggestionGenerator::~CreditCardSuggestionGenerator() = default;

void CreditCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void CreditCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  if (!form_structure || !trigger_autofill_field ||
      trigger_autofill_field->Type().GetCreditCardType() == UNKNOWN_TYPE) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

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
      // `AutofillField::field_modifiers_`.
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

  SuggestionDataSource data_source;
  std::vector<Suggestion> suggestions;
  CreditCardSuggestionSummary summary;
  payments::AmountExtractionStatus amount_extraction_status;
  if (amount_extraction_manager_) {
    amount_extraction_status.has_timed_out_for_page_load =
        amount_extraction_manager_->HasTimedOutForPageLoad();
    amount_extraction_status.seen_unsupported_currency_for_page_load =
        amount_extraction_manager_->SeenUnsupportedCurrencyForPageLoad();
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

  if (IsSaveAndFillEnabled() &&
      ShouldShowCreditCardSaveAndFill(const_cast<AutofillClient&>(client),
                                      is_complete_form, trigger_field)) {
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
                     // `AutofillField::field_modifiers_` after launching.
                     trigger_field.is_autofilled_according_to_renderer(),
                     display_gpay_logo, amount_extraction_status,
                     /*bnpl_manager=*/nullptr));
    data_source = SuggestionDataSource::kSaveAndFillPromo;
  } else if (!virtual_card_guid_to_last_four_map.empty() &&
             !exclude_virtual_cards_) {
    const std::vector<CreditCard> cards_to_suggest =
        FetchVirtualCardStandaloneCvcFieldSuggestionDataSync(
            client, trigger_field, virtual_card_guid_to_last_four_map,
            summary.metadata_logging_context);
    suggestions = GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
        client, trigger_field, virtual_card_guid_to_last_four_map,
        cards_to_suggest, amount_extraction_status);
    data_source = SuggestionDataSource::kVirtualStandaloneCvc;
  } else {
    // If no virtual cards available for standalone CVC field, fall back to
    // regular credit card suggestions.
    const std::u16string autofilled_last_four_digits_in_form_for_filtering =
        is_card_number_autofilled && card_number_field_value.size() >= 4
            ? card_number_field_value.substr(card_number_field_value.size() - 4)
            : u"";

    const std::vector<CreditCard> cards_to_suggest =
        FetchCreditCardOrCvcFieldSuggestionDataSync(
            client, trigger_field,
            trigger_autofill_field->Type().GetCreditCardType(),
            four_digit_combinations_in_dom_.get(),
            autofilled_last_four_digits_in_form_for_filtering, summary);
    suggestions = GenerateCreditCardOrCvcFieldSuggestionsSync(
        client, trigger_field,
        trigger_autofill_field->Type().GetCreditCardType(),
        ShouldShowScanCreditCard(*form_structure, *trigger_autofill_field,
                                 client),
        summary, card_number_field_value.empty(), cards_to_suggest,
        amount_extraction_status, bnpl_manager_);

    data_source = SuggestionDataSource::kCreditCard;
  }

  bool is_virtual_card_standalone_cvc_field =
      std::ranges::any_of(suggestions, [](Suggestion suggestion) {
        return suggestion.type == SuggestionType::kVirtualCreditCardEntry;
      });

  if (credit_card_form_event_logger_) {
    credit_card_form_event_logger_->OnDidFetchSuggestion(
        suggestions, summary.with_cvc,
        summary.with_card_info_retrieval_enrolled,
        summary.with_pay_later_tab_suggestion,
        summary.with_externally_saved_card, summary.with_never_used_card,
        is_virtual_card_standalone_cvc_field,
        std::move(summary.metadata_logging_context));
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

  callback({data_source, std::move(suggestions)});
}

}  // namespace autofill
