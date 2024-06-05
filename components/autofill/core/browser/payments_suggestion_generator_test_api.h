// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_TEST_API_H_

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments_suggestion_generator.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class PaymentsSuggestionGeneratorTestApi {
 public:
  explicit PaymentsSuggestionGeneratorTestApi(
      PaymentsSuggestionGenerator& suggestion_generator)
      : suggestion_generator_(suggestion_generator) {}

  std::vector<CreditCard> GetOrderedCardsToSuggest(
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      bool suppress_disused_cards,
      bool prefix_match,
      bool include_virtual_cards) {
    return suggestion_generator_->GetOrderedCardsToSuggest(
        trigger_field, trigger_field_type, suppress_disused_cards, prefix_match,
        include_virtual_cards);
  }

  Suggestion CreateCreditCardSuggestion(
      const CreditCard& credit_card,
      FieldType trigger_field_type,
      bool virtual_card_option,
      bool card_linked_offer_available,
      url::Origin origin = url::Origin()) const {
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    return suggestion_generator_->CreateCreditCardSuggestion(
        credit_card, trigger_field_type, virtual_card_option,
        card_linked_offer_available, metadata_logging_context);
  }

  Suggestion CreateCreditCardSuggestionWithMetadataContext(
      const CreditCard& credit_card,
      FieldType trigger_field_type,
      bool virtual_card_option,
      bool card_linked_offer_available,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
      url::Origin origin = url::Origin()) const {
    return suggestion_generator_->CreateCreditCardSuggestion(
        credit_card, trigger_field_type, virtual_card_option,
        card_linked_offer_available, metadata_logging_context);
  }

  // TODO(b/326950201): Remove and use GetOrderedCardsToSuggest instead.
  bool ShouldShowVirtualCardOption(const CreditCard* candidate_card) {
    return suggestion_generator_->ShouldShowVirtualCardOption(candidate_card);
  }

 private:
  raw_ref<PaymentsSuggestionGenerator> suggestion_generator_;
};

inline PaymentsSuggestionGeneratorTestApi test_api(
    PaymentsSuggestionGenerator& suggestion_generator) {
  return PaymentsSuggestionGeneratorTestApi(suggestion_generator);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_TEST_API_H_
