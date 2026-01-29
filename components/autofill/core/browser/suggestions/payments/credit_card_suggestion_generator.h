// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_

#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// Fetches SuggestionData, used for credit card or cvc field suggestion
// generation. Fetched data will be used in
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
    const AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    bool exclude_virtual_cards);

// A `SuggestionGenerator` for `FillingProduct::kCreditCard`.
//
// This class encapsulates logic used exclusively for generating credit card
// suggestions. Free functions, that are also used in TouchToFill feature,
// are still shared in payments_suggestion_generator.h file.
class CreditCardSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit CreditCardSuggestionGenerator(
      const std::vector<std::string>& four_digit_combinations_in_dom,
      const payments::AmountExtractionStatus& amount_extraction_status,
      autofill_metrics::CreditCardFormEventLogger*
          credit_card_form_event_logger,
      const AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
      bool exclude_virtual_cards);
  ~CreditCardSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<void(std::pair<SuggestionDataSource,
                                       std::vector<SuggestionData>>)> callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

 private:
  raw_ref<const std::vector<std::string>> four_digit_combinations_in_dom_;
  CreditCardSuggestionSummary summary_;
  raw_ref<const payments::AmountExtractionStatus> amount_extraction_status_;
  raw_ptr<autofill_metrics::CreditCardFormEventLogger>
      credit_card_form_event_logger_;
  AutofillMetrics::PaymentsSigninState signin_state_for_metrics_;
  bool exclude_virtual_cards_ = false;
  base::WeakPtrFactory<CreditCardSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
