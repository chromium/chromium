// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

CreditCardSuggestionGenerator::CreditCardSuggestionGenerator(
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty,
    bool is_complete_form,
    const payments::AmountExtractionStatus& amount_extraction_status)
    : four_digit_combinations_in_dom_(four_digit_combinations_in_dom),
      autofilled_last_four_digits_in_form_for_filtering_(
          autofilled_last_four_digits_in_form_for_filtering),
      should_show_scan_credit_card_(should_show_scan_credit_card),
      summary_(summary),
      is_card_number_field_empty_(is_card_number_field_empty),
      is_complete_form_(is_complete_form),
      amount_extraction_status_(amount_extraction_status) {}

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
  callback(FetchCreditCardSuggestionDataSync(
      const_cast<AutofillClient&>(client), trigger_field,
      trigger_autofill_field->Type().GetCreditCardType(), summary_.get(),
      is_complete_form_, four_digit_combinations_in_dom_.get(),
      autofilled_last_four_digits_in_form_for_filtering_.get()));
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
  callback({FillingProduct::kCreditCard,
            GenerateCreditCardSuggestionsSync(
                const_cast<AutofillClient&>(client), trigger_field,
                trigger_autofill_field->Type().GetCreditCardType(),
                summary_.get(), should_show_scan_credit_card_,
                four_digit_combinations_in_dom_.get(), all_suggestion_data,
                is_card_number_field_empty_, amount_extraction_status_.get())});
}

}  // namespace autofill
