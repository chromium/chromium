// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"

namespace autofill {

MerchantPromoCodeManager::MerchantPromoCodeManager() = default;

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

  auto on_suggestions_generated = base::BindOnce(
      [](SingleFieldFillRouter::OnSuggestionsReturnedCallback& callback,
         bool& suggestions_generated, FieldGlobalId field_id,
         SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions_generated = !returned_suggestions.second.empty();
        if (suggestions_generated) {
          std::move(callback).Run(field_id,
                                  std::move(returned_suggestions.second));
        }
      },
      std::ref(on_suggestions_returned), std::ref(suggestions_generated),
      field.global_id());

  // Since the `on_suggestions_generated` callback is called synchronously, we
  // can assume that `suggestions_generated` will hold the correct value.
  merchant_promo_code_suggestion_generator.GenerateSuggestions(
      form_structure.ToFormData(), field, &form_structure, &autofill_field,
      client, std::move(on_suggestions_generated));
  return suggestions_generated;
}

}  // namespace autofill
