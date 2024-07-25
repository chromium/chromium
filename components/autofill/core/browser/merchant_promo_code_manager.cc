// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/merchant_promo_code_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_suggestion_generator.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"

namespace autofill {

MerchantPromoCodeManager::MerchantPromoCodeManager() = default;

MerchantPromoCodeManager::~MerchantPromoCodeManager() = default;

bool MerchantPromoCodeManager::OnGetSingleFieldSuggestions(
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client,
    OnSuggestionsReturnedCallback on_suggestions_returned) {
  // The field is eligible only if it's focused on a merchant promo code.
  if (!autofill_field ||
      autofill_field->Type().GetStorableType() != MERCHANT_PROMO_CODE) {
    return false;
  }

  // If merchant promo code offers are available for the given site, and the
  // profile is not OTR, show the promo code offers.
  if (!is_off_the_record_ && personal_data_manager_) {
    const std::vector<const AutofillOfferData*> promo_code_offers =
        personal_data_manager_->payments_data_manager()
            .GetActiveAutofillPromoCodeOffersForOrigin(
                form_structure->main_frame_origin().GetURL());
    if (!promo_code_offers.empty()) {
      SendPromoCodeSuggestions(std::move(promo_code_offers), field,
                               std::move(on_suggestions_returned));
      return true;
    }
  }
  return false;
}

void MerchantPromoCodeManager::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {}

void MerchantPromoCodeManager::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    SuggestionType type) {}

void MerchantPromoCodeManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  uma_recorder_.OnOfferSuggestionSelected(suggestion.type);
}

void MerchantPromoCodeManager::Init(PersonalDataManager* personal_data_manager,
                                    bool is_off_the_record) {
  personal_data_manager_ = personal_data_manager;
  is_off_the_record_ = is_off_the_record;
}

void MerchantPromoCodeManager::UMARecorder::OnOffersSuggestionsShown(
    const FieldGlobalId& field_global_id,
    const std::vector<const AutofillOfferData*>& offers) {
  // Log metrics related to the showing of overall offers suggestions popup.
  autofill_metrics::LogOffersSuggestionsPopupShown(
      /*first_time_being_logged=*/
      most_recent_suggestions_shown_field_global_id_ != field_global_id);

  // Log metrics related to the showing of individual offers in the offers
  // suggestions popup.
  for (const AutofillOfferData* offer : offers) {
    // We log every time an individual offer suggestion is shown, regardless if
    // the user is repeatedly clicking the same field.
    autofill_metrics::LogIndividualOfferSuggestionEvent(
        autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShown,
        offer->GetOfferType());

    // We log that this individual offer suggestion was shown once for this
    // field while autofilling if it is the first time being logged.
    if (most_recent_suggestions_shown_field_global_id_ != field_global_id) {
      autofill_metrics::LogIndividualOfferSuggestionEvent(
          autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShownOnce,
          offer->GetOfferType());
    }
  }

  most_recent_suggestions_shown_field_global_id_ = field_global_id;
}

void MerchantPromoCodeManager::UMARecorder::OnOfferSuggestionSelected(
    SuggestionType type) {
  if (type == SuggestionType::kMerchantPromoCodeEntry) {
    // We log every time an individual offer suggestion is selected, regardless
    // if the user is repeatedly autofilling the same field.
    autofill_metrics::LogIndividualOfferSuggestionEvent(
        autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionSelected,
        AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER);

    // We log that this individual offer suggestion was selected once for this
    // field while autofilling if it is the first time being logged.
    if (most_recent_suggestion_selected_field_global_id_ !=
        most_recent_suggestions_shown_field_global_id_) {
      autofill_metrics::LogIndividualOfferSuggestionEvent(
          autofill_metrics::OffersSuggestionsEvent::
              kOfferSuggestionSelectedOnce,
          AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER);
    }
  } else if (type == SuggestionType::kSeePromoCodeDetails) {
    // We log every time the see offer details suggestion in the footer is
    // selected, regardless if the user is repeatedly autofilling the same
    // field.
    autofill_metrics::LogIndividualOfferSuggestionEvent(
        autofill_metrics::OffersSuggestionsEvent::
            kOfferSuggestionSeeOfferDetailsSelected,
        AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER);

    // We log that this individual see offer details suggestion in the footer
    // was selected once for this field while autofilling if it is the first
    // time being logged.
    if (most_recent_suggestion_selected_field_global_id_ !=
        most_recent_suggestions_shown_field_global_id_) {
      autofill_metrics::LogIndividualOfferSuggestionEvent(
          autofill_metrics::OffersSuggestionsEvent::
              kOfferSuggestionSeeOfferDetailsSelectedOnce,
          AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER);
    }
  }

  most_recent_suggestion_selected_field_global_id_ =
      most_recent_suggestions_shown_field_global_id_;
}

void MerchantPromoCodeManager::SendPromoCodeSuggestions(
    std::vector<const AutofillOfferData*> promo_code_offers,
    const FormFieldData& field,
    OnSuggestionsReturnedCallback on_suggestions_returned) {
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

  // Log that promo code autofill suggestions were shown.
  uma_recorder_.OnOffersSuggestionsShown(field.global_id(), promo_code_offers);
}

}  // namespace autofill
