// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include <algorithm>

#include "base/check_deref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {
AutofillOfferManager::AutofillOfferManager(
    PaymentsDataManager* payments_data_manager)
    : payments_data_manager_(CHECK_DEREF(payments_data_manager)) {}

AutofillOfferManager::~AutofillOfferManager() = default;

void AutofillOfferManager::OnDidNavigateFrame(AutofillClient& client) {
  UpdateOfferNotificationVisibility(client);
}

void AutofillOfferManager::UpdateOfferNotificationVisibility(
    AutofillClient& client) {
  notification_handler_.UpdateOfferNotificationVisibility(client);
}

bool AutofillOfferManager::IsUrlEligible(
    const GURL& last_committed_primary_main_frame_url) {
  const GURL origin =
      last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL();
  return std::ranges::any_of(payments_data_manager_->GetAutofillOffers(),
                             [&](const AutofillOfferData* offer) {
                               return std::ranges::contains(
                                   offer->GetMerchantOrigins(), origin);
                             });
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

}  // namespace autofill
