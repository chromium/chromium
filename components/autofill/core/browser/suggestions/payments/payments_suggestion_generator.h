// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Generates suggestions for all available credit cards based on the
// `trigger_field_type`, `trigger_field` and `four_digit_combinations_in_dom`.
// `summary` contains metadata about the returned suggestions.
// TODO(crbug.com/448688721): Consolidate the input parameters.
std::vector<Suggestion> GetSuggestionsForCreditCards(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const AutofillField& autofill_trigger_field,
    AutofillClient& client,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const payments::AmountExtractionStatus& amount_extraction_status,
    autofill_metrics::CreditCardFormEventLogger& credit_card_form_event_logger,
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics);

// Helper function, that implements "Fetch" phase of the
// GetSuggestionsForCreditCards function.
std::pair<SuggestionGenerator::SuggestionDataSource,
          std::vector<SuggestionGenerator::SuggestionData>>
FetchCreditCardSuggestionDataSync(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure& form_structure,
    const AutofillField& autofill_trigger_field,
    AutofillClient& client,
    FieldType trigger_field_type,
    CreditCardSuggestionSummary& summary,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    autofill_metrics::CreditCardFormEventLogger& credit_card_form_event_logger,
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics);

// Helper function, that implements "Generate" phase of the
// GetSuggestionsForCreditCards function.
std::vector<Suggestion> GenerateCreditCardSuggestionsSync(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure& form_structure,
    const AutofillField& autofill_trigger_field,
    AutofillClient& client,
    FieldType trigger_field_type,
    CreditCardSuggestionSummary& summary,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const base::flat_map<SuggestionGenerator::SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>&
        suggestion_data,
    const payments::AmountExtractionStatus& amount_extraction_status,
    autofill_metrics::CreditCardFormEventLogger& credit_card_form_event_logger);

// Fetches SuggestionData, used for credit card or cvc field suggestion
// generation. Fetched data wil be used in
// GenerateCreditCardOrCvcFieldSuggestionsSync.
std::pair<SuggestionGenerator::SuggestionDataSource,
          std::vector<SuggestionGenerator::SuggestionData>>
FetchCreditCardOrCvcFieldSuggestionDataSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    CreditCardSuggestionSummary& summary);

// Generates suggestions for all available credit cards based on the
// `trigger_field_type` and `trigger_field`. `summary` contains metadata about
// the returned suggestions. `last_four_set_for_cvc_suggestion_filtering` is a
// set of card number last four that will be used for suggestion filtering. this
// is used to avoid showing suggestions that is unrelated to the cards that have
// already been autofilled in the form.
// `is_card_number_field_empty` indicates whether the card number field is empty
// after the value inside of it is sanitized. this is used to decide whether the
// bnpl suggestion should be appended together with the credit card suggestions.
// todo(crbug.com/40916587): implement last four extraction from the dom.
// todo(crbug.com/448688721): Consolidate the input parameters.
std::vector<Suggestion> GenerateCreditCardOrCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty,
    const base::flat_map<SuggestionGenerator::SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>&
        suggestion_data,
    const payments::AmountExtractionStatus& amount_extraction_status);
// Fetches SuggestionData, used for standalone CVC fields suggestion generation.
// Fetched data wil be used in
// GenerateVirtualCardStandaloneCvcFieldSuggestionsSync.
std::pair<SuggestionGenerator::SuggestionDataSource,
          std::vector<SuggestionGenerator::SuggestionData>>
FetchVirtualCardStandaloneCvcFieldSuggestionDataSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

// Generates suggestions for standalone CVC fields. These only apply to
// virtual cards that are saved on file to a merchant. In these cases,
// we only display the virtual card option and do not show FPAN option.
std::vector<Suggestion> GenerateVirtualCardStandaloneCvcFieldSuggestionsSync(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const base::flat_map<std::string,
                         VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map,
    const base::flat_map<SuggestionGenerator::SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>&
        suggestion_data,
    const payments::AmountExtractionStatus& amount_extraction_status);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_H_
