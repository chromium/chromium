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

namespace payments {
class AmountExtractionManager;
class BnplManager;
}  // namespace payments

// Generates suggestions for all available credit cards based on the
// `trigger_field_type`, `trigger_field` and `four_digit_combinations_in_dom`.
// `summary` contains metadata about the returned suggestions.
// This function is a thin wrapper calling both FetchSuggestionData() and
// GenerateSuggestions() from the same instance of type
// CreditCardSuggestionGenerator. Additional logic should not be added
// to this function but to either of the other two.
// TODO(crbug.com/448688721): Consolidate the input parameters.
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
    bool exclude_virtual_cards);

std::vector<Suggestion> GetSuggestionsForBnpl(
    std::vector<payments::BnplIssuerContext> issuer_contexts,
    const std::string& app_locale,
    const bool is_card_number_field_empty);

Suggestion GetLoadingSuggestionForPayLaterTab(
    int expected_number_of_suggestions);

// A `SuggestionGenerator` for `FillingProduct::kCreditCard`.
//
// This class encapsulates logic used exclusively for generating credit card
// suggestions. Free functions, that are also used in TouchToFill feature,
// are still shared in payments_suggestion_generator.h file.
class CreditCardSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit CreditCardSuggestionGenerator(
      const std::vector<std::string>& four_digit_combinations_in_dom,
      payments::AmountExtractionManager* amount_extraction_manager,
      payments::BnplManager* bnpl_manager,
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
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map_;
  CreditCardSuggestionSummary summary_;
  raw_ptr<payments::AmountExtractionManager> amount_extraction_manager_;
  raw_ptr<payments::BnplManager> bnpl_manager_;
  raw_ptr<autofill_metrics::CreditCardFormEventLogger>
      credit_card_form_event_logger_;
  AutofillMetrics::PaymentsSigninState signin_state_for_metrics_;
  bool exclude_virtual_cards_ = false;
  base::WeakPtrFactory<CreditCardSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
