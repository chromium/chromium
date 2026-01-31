// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Converts the vector of promo code offers that is passed in to a vector of
// suggestions that can be displayed to the user for a promo code field.
std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
    const std::vector<const AutofillOfferData*>& promo_code_offers) {
  std::vector<Suggestion> suggestions;
  GURL footer_offer_details_url;
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

    // Every offer for a given merchant leads to the same GURL, so we grab the
    // first offer's offer details url as the payload for the footer to set
    // later.
    if (footer_offer_details_url.is_empty() &&
        !promo_code_offer->GetOfferDetailsUrl().is_empty() &&
        promo_code_offer->GetOfferDetailsUrl().is_valid()) {
      footer_offer_details_url = promo_code_offer->GetOfferDetailsUrl();
    }
  }

  // Ensure that there are suggestions and that we were able to find at least
  // one suggestion with a valid offer details url before adding the footer.
  DCHECK(suggestions.size() > 0);
  if (!footer_offer_details_url.is_empty()) {
    // Add the footer separator since we will now have a footer in the offers
    // suggestions popup.
    suggestions.emplace_back(SuggestionType::kSeparator);

    // Add the footer suggestion that navigates the user to the promo code
    // details page in the offers suggestions popup.
    suggestions.emplace_back(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT),
        SuggestionType::kSeePromoCodeDetails);
    Suggestion& suggestion = suggestions.back();

    // We set the payload for the footer as |footer_offer_details_url|, which is
    // the offer details url of the first offer we had for this merchant. We
    // will navigate to the url in |footer_offer_details_url| if the footer is
    // selected in AutofillExternalDelegate::DidAcceptSuggestion().
    suggestion.payload = std::move(footer_offer_details_url);
    suggestion.trailing_icon = Suggestion::Icon::kGoogle;
  }
  return suggestions;
}

}  // namespace

MerchantPromoCodeSuggestionGenerator::MerchantPromoCodeSuggestionGenerator() =
    default;
MerchantPromoCodeSuggestionGenerator::~MerchantPromoCodeSuggestionGenerator() =
    default;

void MerchantPromoCodeSuggestionGenerator::FetchSuggestionData(
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

void MerchantPromoCodeSuggestionGenerator::GenerateSuggestions(
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

void MerchantPromoCodeSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
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

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(promo_code_offers),
      [](const AutofillOfferData* offer) { return SuggestionData(*offer); });
  callback({SuggestionDataSource::kMerchantPromoCode, suggestion_data});
}

void MerchantPromoCodeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  auto it = all_suggestion_data.find(SuggestionDataSource::kMerchantPromoCode);
  std::vector<SuggestionData> promo_code_suggestion_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();
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
