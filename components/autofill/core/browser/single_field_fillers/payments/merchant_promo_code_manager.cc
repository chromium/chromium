// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

MerchantPromoCodeManager::MerchantPromoCodeManager(
    AutofillClient* autofill_client) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableWalletDirectOffers)) {
    autofill_managers_observation_.Observe(
        autofill_client, ScopedAutofillManagersObservation::
                             InitializationPolicy::kObservePreexistingManagers);
  }
}

MerchantPromoCodeManager::~MerchantPromoCodeManager() = default;

void MerchantPromoCodeManager::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  const FormStructure* form_structure = manager.FindCachedFormById(form);
  if (!form_structure) {
    return;
  }

  auto promo_code_field = std::ranges::find_if(
      form_structure->fields(),
      [](const std::unique_ptr<AutofillField>& field) {
        return field->Type().GetTypes().contains(MERCHANT_PROMO_CODE) &&
               field->is_visible();
      });
  if (promo_code_field == form_structure->fields().end()) {
    return;
  }

  promo_code_form_global_ids_.insert(form);

  if (!manager.client().GetPaymentsAutofillClient()) {
    return;
  }

  std::vector<const AutofillOfferData*> promo_code_offers =
      manager.client()
          .GetPaymentsAutofillClient()
          ->GetPaymentsDataManager()
          .GetActiveAutofillPromoCodeOffersForOrigin(
              manager.client()
                  .GetLastCommittedPrimaryMainFrameOrigin()
                  .GetURL());

  // Log the page load funnel event on the first detection of a visible promo
  // code field on the current page.
  if (!page_metrics_.has_logged_if_promo_code_exists_for_page) {
    page_metrics_.has_logged_if_promo_code_exists_for_page = true;
    autofill_metrics::LogPromoCodeFormEvent(
        promo_code_offers.empty() ? autofill_metrics::PromoCodeFormEvent::
                                        kNavigatedToPageWithoutPromoCode
                                  : autofill_metrics::PromoCodeFormEvent::
                                        kNavigatedToPageWithPromoCode);
  }

  if (promo_code_offers.empty()) {
    return;
  }

  manager.client().ShowAutofillFieldIphForFeature(
      **promo_code_field, AutofillClient::IphFeature::kWalletDirectOffers);
}

void MerchantPromoCodeManager::OnBeforeFormSubmitted(AutofillManager& manager,
                                                     const FormData& form) {
  // Submissions are only logged once per page load, and only on pages with a
  // visible promo code field.
  if (promo_code_form_global_ids_.empty() ||
      page_metrics_.has_logged_form_submitted) {
    return;
  }

  AutofillClient& client = manager.client();

  // Only credit card form submissions are logged.
  const FormStructure* form_structure =
      manager.FindCachedFormById(form.global_id());
  if (!form_structure ||
      !form_structure->GetFormTypes(GetAcUnrecognizedBehavior(client))
           .contains(FormType::kCreditCardForm)) {
    return;
  }

  page_metrics_.has_logged_form_submitted = true;

  if (HasPromoCodeFieldWithValue(manager, form)) {
    autofill_metrics::LogPromoCodeFormEvent(
        autofill_metrics::PromoCodeFormEvent::
            kFormSubmittedWithPromoCodeFieldContainingValue);
  }

  if (page_metrics_.has_logged_suggestion_filled) {
    autofill_metrics::LogPromoCodeFormEvent(
        autofill_metrics::PromoCodeFormEvent::
            kFormSubmittedAfterPromoCodeFilled);
  }

  payments::PaymentsAutofillClient* payments_client =
      client.GetPaymentsAutofillClient();
  const bool has_promo_code_for_page =
      payments_client &&
      !payments_client->GetPaymentsDataManager()
           .GetActiveAutofillPromoCodeOffersForOrigin(
               client.GetLastCommittedPrimaryMainFrameOrigin().GetURL())
           .empty();

  if (has_promo_code_for_page) {
    autofill_metrics::LogPromoCodeFormEvent(
        autofill_metrics::PromoCodeFormEvent::
            kFormSubmittedWithPromoCodePresentForPage);
  } else {
    autofill_metrics::LogPromoCodeFormEvent(
        autofill_metrics::PromoCodeFormEvent::
            kFormSubmittedWithoutPromoCodePresentForPage);
  }
}

bool MerchantPromoCodeManager::OnGetSingleFieldSuggestions(
    const FormStructure& form_structure,
    const FormFieldData& field,
    const AutofillField& autofill_field,
    AutofillClient& client,
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

void MerchantPromoCodeManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  CHECK_EQ(suggestion.type, SuggestionType::kMerchantPromoCodeEntry);
  if (page_metrics_.has_logged_suggestion_filled) {
    return;
  }

  page_metrics_.has_logged_suggestion_filled = true;
  autofill_metrics::LogPromoCodeFormEvent(
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionFilled);
}

void MerchantPromoCodeManager::DidShowSuggestions() {
  if (page_metrics_.has_logged_suggestions_shown) {
    return;
  }

  page_metrics_.has_logged_suggestions_shown = true;
  autofill_metrics::LogPromoCodeFormEvent(
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionsShown);
}

void MerchantPromoCodeManager::Reset() {
  page_metrics_ = {};
  promo_code_form_global_ids_.clear();
}

bool MerchantPromoCodeManager::HasPromoCodeFieldWithValue(
    AutofillManager& manager,
    const FormData& submitted_form) const {
  for (FormGlobalId form_id : promo_code_form_global_ids_) {
    const FormStructure* form_structure = manager.FindCachedFormById(form_id);
    if (!form_structure) {
      continue;
    }
    for (const std::unique_ptr<AutofillField>& field :
         form_structure->fields()) {
      if (!field->Type().GetTypes().contains(MERCHANT_PROMO_CODE)) {
        continue;
      }
      // The cached form is not updated with the submitted values yet, so the
      // submitted value is used if the field is part of the submitted form.
      const FormFieldData* submitted_field =
          submitted_form.FindFieldByGlobalId(field->global_id());
      const std::u16string& value =
          submitted_field ? submitted_field->value() : field->value();
      if (!base::TrimWhitespace(value, base::TRIM_ALL).empty()) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace autofill
