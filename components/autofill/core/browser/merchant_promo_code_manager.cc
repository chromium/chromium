// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/merchant_promo_code_manager.h"

#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"

namespace autofill {

MerchantPromoCodeManager::MerchantPromoCodeManager() = default;

MerchantPromoCodeManager::~MerchantPromoCodeManager() = default;

void MerchantPromoCodeManager::OnGetSingleFieldSuggestions(
    int query_id,
    bool is_autocomplete_enabled,
    bool autoselect_first_suggestion,
    const std::u16string& name,
    const std::u16string& prefix,
    const std::string& form_control_type,
    base::WeakPtr<SuggestionsHandler> handler,
    const SuggestionsContext& context) {
  // If merchant promo code offers are available for the given site, and the
  // profile is not OTR, show the promo code offers.
  if (!is_off_the_record_ && personal_data_manager_) {
    std::vector<const AutofillOfferData*> promo_code_offers =
        personal_data_manager_->GetActiveAutofillPromoCodeOffersForOrigin(
            context.form_structure->main_frame_origin().GetURL());
    if (!promo_code_offers.empty()) {
      SendPromoCodeSuggestions(
          promo_code_offers,
          QueryHandler(query_id, autoselect_first_suggestion, prefix, handler));
      return;
    }
  }
}

void MerchantPromoCodeManager::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {}

void MerchantPromoCodeManager::CancelPendingQueries(
    const SuggestionsHandler* handler) {}

void MerchantPromoCodeManager::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    int frontend_id) {}

void MerchantPromoCodeManager::OnSingleFieldSuggestionSelected(
    const std::u16string& value,
    int frontend_id) {
  // TODO(crbug.com/1190334): Add promo code suggestion accepted metrics here.
}

void MerchantPromoCodeManager::Init(
    raw_ptr<PersonalDataManager> personal_data_manager,
    bool is_off_the_record) {
  personal_data_manager_ = personal_data_manager;
  is_off_the_record_ = is_off_the_record;
}

base::WeakPtr<MerchantPromoCodeManager> MerchantPromoCodeManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void MerchantPromoCodeManager::SendPromoCodeSuggestions(
    const std::vector<const AutofillOfferData*>& promo_code_offers,
    const QueryHandler& query_handler) {
  if (!query_handler.handler_) {
    // Either the handler has been destroyed, or it is invalid.
    return;
  }

  // If the input box content equals any of the available promo codes, then
  // assume the promo code has been filled, and don't show any suggestions.
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    if (query_handler.prefix_ ==
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode())) {
      return;
    }
  }

  // Return suggestions to query handler.
  query_handler.handler_->OnSuggestionsReturned(
      query_handler.client_query_id_,
      query_handler.autoselect_first_suggestion_,
      AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
          promo_code_offers));
}

}  // namespace autofill
