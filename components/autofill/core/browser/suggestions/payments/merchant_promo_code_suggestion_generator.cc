// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"

namespace autofill {
namespace {

// Converts the vector of promo code offers that is passed in to a vector of
// suggestions that can be displayed to the user for a promo code field.
std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
    const std::vector<const AutofillOfferData*>& promo_code_offers) {
  if (promo_code_offers.empty()) {
    return {};
  }

  std::vector<Suggestion> suggestions;
  suggestions.reserve(promo_code_offers.size());
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    // For each promo code, create a suggestion.
    suggestions.emplace_back(
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode()),
        SuggestionType::kMerchantPromoCodeEntry);
    Suggestion& suggestion = suggestions.back();
    if (!promo_code_offer->GetDisplayStrings().value_prop_text.empty()) {
      suggestion.labels = {{Suggestion::Text(base::ASCIIToUTF16(
          promo_code_offer->GetDisplayStrings().value_prop_text))}};
    }
    suggestion.payload =
        Suggestion::Guid(base::NumberToString(promo_code_offer->GetOfferId()));
  }
  return suggestions;
}

}  // namespace

MerchantPromoCodeSuggestionGenerator::MerchantPromoCodeSuggestionGenerator() =
    default;
MerchantPromoCodeSuggestionGenerator::~MerchantPromoCodeSuggestionGenerator() =
    default;

void MerchantPromoCodeSuggestionGenerator::GenerateSuggestions(
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

void MerchantPromoCodeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  // The field is eligible only if it's focused on a merchant promo code.
  if (!form_structure || !trigger_autofill_field ||
      !trigger_autofill_field->Type().GetTypes().contains(
          MERCHANT_PROMO_CODE)) {
    callback({SuggestionDataSource::kMerchantPromoCode, {}});
    return;
  }

  // If merchant promo code offers are available for the given site, and the
  // profile is not OTR, show the promo code offers.
  if (client.IsOffTheRecord() || !client.GetPaymentsAutofillClient()) {
    callback({SuggestionDataSource::kMerchantPromoCode, {}});
    return;
  }
  const std::vector<const AutofillOfferData*> promo_code_offers =
      client.GetPaymentsAutofillClient()
          ->GetPaymentsDataManager()
          .GetActiveAutofillPromoCodeOffersForOrigin(
              form_structure->main_frame_origin().GetURL());

  // If the input box content equals any of the available promo codes, then
  // assume the promo code has been filled, and don't show any suggestions.
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    if (trigger_autofill_field->value() ==
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode())) {
      callback({SuggestionDataSource::kMerchantPromoCode, {}});
      return;
    }
  }

  callback({SuggestionDataSource::kMerchantPromoCode,
            GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers)});
}

}  // namespace autofill
