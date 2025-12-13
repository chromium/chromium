// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_TEST_API_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/virtual_card_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class CreditCardSuggestionGeneratorTestApi {
 public:
  explicit CreditCardSuggestionGeneratorTestApi(
      CreditCardSuggestionGenerator* generator)
      : generator_(generator) {}
  std::vector<CreditCard> FetchCreditCardsForCreditCardOrCvcField(
      const AutofillClient& client,
      const FormFieldData& trigger_field,
      const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
      FieldType trigger_field_type,
      bool should_show_scan_credit_card) {
    return generator_->FetchCreditCardsForCreditCardOrCvcField(
        client, trigger_field,
        autofilled_last_four_digits_in_form_for_filtering, trigger_field_type,
        should_show_scan_credit_card);
  }

  std::vector<Suggestion> GenerateSuggestionsForCreditCardOrCvcField(
      const FormFieldData& trigger_field,
      const FieldType& trigger_field_type,
      const AutofillClient& client,
      const std::vector<CreditCard>& credit_cards,
      CreditCardSuggestionSummary& summary,
      bool should_show_scan_credit_card,
      bool is_card_number_field_empty) {
    return generator_->GenerateSuggestionsForCreditCardOrCvcField(
        trigger_field, trigger_field_type, client, credit_cards, summary,
        should_show_scan_credit_card, is_card_number_field_empty);
  }

  std::vector<CreditCard> FetchVirtualCardsForStandaloneCvcField(
      const AutofillClient& client,
      const FormFieldData& trigger_field,
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
          virtual_card_guid_to_last_four_map) {
    return generator_->FetchVirtualCardsForStandaloneCvcField(
        client, trigger_field, virtual_card_guid_to_last_four_map);
  }

  std::vector<Suggestion> GenerateSuggestionsForStandaloneCvcField(
      const FormFieldData& trigger_field,
      const AutofillClient& client,
      const std::vector<VirtualCardSuggestionData>& credit_cards,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
    return generator_->GenerateSuggestionsForStandaloneCvcField(
        trigger_field, client, credit_cards, metadata_logging_context);
  }

  std::vector<Suggestion> GetCreditCardFooterSuggestions(
      const AutofillClient& client,
      bool should_show_bnpl_suggestion,
      bool should_show_scan_credit_card,
      bool is_autofilled,
      bool with_gpay_logo) {
    return generator_->GetCreditCardFooterSuggestions(
        client, should_show_bnpl_suggestion, should_show_scan_credit_card,
        is_autofilled, with_gpay_logo);
  }

  Suggestion CreateCreditCardSuggestion(
      const CreditCard& credit_card,
      const AutofillClient& client,
      FieldType trigger_field_type,
      bool virtual_card_option,
      bool card_linked_offer_available,
      base::optional_ref<autofill_metrics::CardMetadataLoggingContext>
          metadata_logging_context = std::nullopt) {
    autofill_metrics::CardMetadataLoggingContext dummy_context;
    return generator_->CreateCreditCardSuggestion(
        credit_card, client, trigger_field_type, virtual_card_option,
        card_linked_offer_available,
        metadata_logging_context.has_value() ? metadata_logging_context.value()
                                             : dummy_context);
  }

 private:
  raw_ptr<CreditCardSuggestionGenerator> generator_;
};

inline CreditCardSuggestionGeneratorTestApi test_api(
    CreditCardSuggestionGenerator& generator) {
  return CreditCardSuggestionGeneratorTestApi(&generator);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_TEST_API_H_
