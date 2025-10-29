// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"

namespace autofill {

MerchantPromoCodeManager::MerchantPromoCodeManager(
    PaymentsDataManager* payments_data_manager,
    bool is_off_the_record)
    : payments_data_manager_(payments_data_manager),
      is_off_the_record_(is_off_the_record) {}

MerchantPromoCodeManager::~MerchantPromoCodeManager() = default;

bool MerchantPromoCodeManager::OnGetSingleFieldSuggestions(
    const FormStructure& form_structure,
    const FormFieldData& field,
    const AutofillField& autofill_field,
    const AutofillClient& client,
    SingleFieldFillRouter::OnSuggestionsReturnedCallback&
        on_suggestions_returned) {
  MerchantPromoCodeSuggestionGenerator merchant_promo_code_suggestion_generator;
  bool suggestions_generated = false;

  auto on_suggestions_generated =
      [&on_suggestions_returned, &field, &suggestions_generated](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions_generated = !returned_suggestions.second.empty();
        if (suggestions_generated) {
          std::move(on_suggestions_returned)
              .Run(field.global_id(), std::move(returned_suggestions.second));
        }
      };

  auto on_suggestion_data_returned =
      [&on_suggestions_generated, &field, &form_structure, &autofill_field,
       &client, &merchant_promo_code_suggestion_generator](
          std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>>
              suggestion_data) {
        merchant_promo_code_suggestion_generator.GenerateSuggestions(
            form_structure.ToFormData(), field, &form_structure,
            &autofill_field, client, {std::move(suggestion_data)},
            on_suggestions_generated);
      };

  // Since the `on_suggestion_data_returned` callback is called synchronously,
  // we can assume that `suggestions_generated` will hold correct value.
  merchant_promo_code_suggestion_generator.FetchSuggestionData(
      form_structure.ToFormData(), field, &form_structure, &autofill_field,
      client, on_suggestion_data_returned);
  return suggestions_generated;
}

void MerchantPromoCodeManager::SendPromoCodeSuggestions(
    std::vector<const AutofillOfferData*> promo_code_offers,
    const FormFieldData& field,
    SingleFieldFillRouter::OnSuggestionsReturnedCallback
        on_suggestions_returned) {
  // If the input box content equals any of the available promo codes, then
  // assume the promo code has been filled, and don't show any suggestions.
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    if (field.value() == base::ASCIIToUTF16(promo_code_offer->GetPromoCode())) {
      std::move(on_suggestions_returned).Run(field.global_id(), {});
      return;
    }
  }

  std::move(on_suggestions_returned)
      .Run(field.global_id(),
           GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers));
}

}  // namespace autofill
