// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/i18n/time_formatting.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
  suggestions.reserve(promo_code_offers.size() + 2);
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    // For each promo code, create a suggestion.
    std::u16string main_text = base::UTF8ToUTF16(
        promo_code_offer->GetDisplayStrings().value_prop_text);
    Suggestion& suggestion = suggestions.emplace_back(
        main_text, SuggestionType::kMerchantPromoCodeEntry);
    suggestion.icon = Suggestion::Icon::kOfferTag;

    std::vector<std::vector<Suggestion::Text>> labels;
    labels.reserve(2);

    std::u16string code_label = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_PROMO_CODE_SUGGESTION_CODE_LABEL,
        base::UTF8ToUTF16(promo_code_offer->GetPromoCode()));
    labels.emplace_back(
        std::vector<Suggestion::Text>{Suggestion::Text(code_label)});

    std::u16string expiration_date =
        base::TimeFormatShortDate(promo_code_offer->GetExpiry());
    labels.emplace_back(std::vector<Suggestion::Text>{
        Suggestion::Text(l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_OFFERS_EXPIRES_ON, expiration_date))});

    suggestion.labels = std::move(labels);
    suggestion.payload =
        Suggestion::PromoCode(promo_code_offer->GetPromoCode());
  }

  suggestions.emplace_back(SuggestionType::kSeparator);
  suggestions.emplace_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_OFFERS_FOOTER_TEXT),
      SuggestionType::kManageOffers);
  Suggestion& manage_offers_suggestion = suggestions.back();
  manage_offers_suggestion.icon = Suggestion::Icon::kSettings;
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
  const PaymentsDataManager& payments_data_manager =
      client.GetPaymentsAutofillClient()->GetPaymentsDataManager();

  const std::vector<const AutofillOfferData*> promo_code_offers =
      payments_data_manager.GetActiveAutofillPromoCodeOffersForOrigin(
          form_structure->main_frame_origin().GetURL());

  // If the input box content equals any of the available promo codes, then
  // assume the promo code has been filled, and don't show any suggestions.
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    if (trigger_autofill_field->value() ==
        base::UTF8ToUTF16(promo_code_offer->GetPromoCode())) {
      callback({SuggestionDataSource::kMerchantPromoCode, {}});
      return;
    }
  }

  callback({SuggestionDataSource::kMerchantPromoCode,
            GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers)});
}

}  // namespace autofill
