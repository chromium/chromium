// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"


#include "base/check_deref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {
AutofillOfferManager::AutofillOfferManager(
    PaymentsDataManager* payments_data_manager)
    : payments_data_manager_(CHECK_DEREF(payments_data_manager)) {
  payments_data_manager_observation.Observe(payments_data_manager);
  UpdateEligibleMerchantDomains();
}

AutofillOfferManager::~AutofillOfferManager() = default;

void AutofillOfferManager::OnPaymentsDataChanged() {
  UpdateEligibleMerchantDomains();
}

void AutofillOfferManager::OnDidNavigateFrame(AutofillClient& client) {
  notification_handler_.UpdateOfferNotificationVisibility(client);
}

bool AutofillOfferManager::IsUrlEligible(
    const GURL& last_committed_primary_main_frame_url) {
  return eligible_merchant_domains_.contains(
      last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL());
}

const AutofillOfferData* AutofillOfferManager::GetOfferForUrl(
    const GURL& last_committed_primary_main_frame_url) const {
  for (const AutofillOfferData* offer :
       payments_data_manager_->GetAutofillOffers()) {
    if (offer->IsActiveAndEligibleForOrigin(
            last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL())) {
      return offer;
    }
  }

  return nullptr;
}

void AutofillOfferManager::UpdateEligibleMerchantDomains() {
  eligible_merchant_domains_.clear();
  for (const AutofillOfferData* offer :
       payments_data_manager_->GetAutofillOffers()) {
    eligible_merchant_domains_.insert(offer->GetMerchantOrigins().begin(),
                                      offer->GetMerchantOrigins().end());
  }
}

}  // namespace autofill
