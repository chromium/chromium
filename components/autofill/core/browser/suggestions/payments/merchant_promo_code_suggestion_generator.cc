// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "base/functional/function_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"

namespace autofill {

MerchantPromoCodeSuggestionGenerator::MerchantPromoCodeSuggestionGenerator() =
    default;
MerchantPromoCodeSuggestionGenerator::~MerchantPromoCodeSuggestionGenerator() =
    default;

void MerchantPromoCodeSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form_data, field_data, form, field, client,
      [&callback](std::pair<FillingProduct,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void MerchantPromoCodeSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form_data, field_data, form, field, all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void MerchantPromoCodeSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  // The field is eligible only if it's focused on a merchant promo code.
  if (!form || !field ||
      !field->Type().GetTypes().contains(MERCHANT_PROMO_CODE)) {
    callback({FillingProduct::kMerchantPromoCode, {}});
    return;
  }

  // If merchant promo code offers are available for the given site, and the
  // profile is not OTR, show the promo code offers.
  if (client.IsOffTheRecord() || !client.GetPaymentsAutofillClient()) {
    callback({FillingProduct::kMerchantPromoCode, {}});
    return;
  }
  const std::vector<const AutofillOfferData*> promo_code_offers =
      client.GetPaymentsAutofillClient()
          ->GetPaymentsDataManager()
          .GetActiveAutofillPromoCodeOffersForOrigin(
              form->main_frame_origin().GetURL());

  // If the input box content equals any of the available promo codes, then
  // assume the promo code has been filled, and don't show any suggestions.
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    if (field->value() ==
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode())) {
      callback({FillingProduct::kMerchantPromoCode, {}});
      return;
    }
  }

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(promo_code_offers),
      [](const AutofillOfferData* offer) { return SuggestionData(*offer); });
  callback({FillingProduct::kMerchantPromoCode, suggestion_data});
}

void MerchantPromoCodeSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  std::vector<SuggestionData> promo_code_suggestion_data =
      ExtractSuggestionDataForFillingProduct(
          all_suggestion_data, FillingProduct::kMerchantPromoCode);
  if (promo_code_suggestion_data.empty()) {
    callback({FillingProduct::kMerchantPromoCode, {}});
    return;
  }

  std::vector<AutofillOfferData> promo_code_offers =
      base::ToVector(std::move(promo_code_suggestion_data),
                     [](SuggestionData& suggestion_data) {
                       return std::get<autofill::AutofillOfferData>(
                           std::move(suggestion_data));
                     });
  std::vector<const AutofillOfferData*> promo_code_offers_ptrs =
      base::ToVector(std::move(promo_code_offers),
                     [](const AutofillOfferData& offer) { return &offer; });
  callback(
      {FillingProduct::kMerchantPromoCode,
       GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers_ptrs)});
}

}  // namespace autofill
