// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/offer_notification_handler.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "url/gurl.h"

namespace autofill {

namespace {

bool IsOfferValid(AutofillOfferData* offer) {
  if (!offer) {
    return false;
  }

  if (offer->GetMerchantOrigins().empty()) {
    return false;
  }

  if (offer->GetOfferType() == AutofillOfferData::OfferType::UNKNOWN) {
    return false;
  }

  return true;
}

}  // namespace

OfferNotificationHandler::OfferNotificationHandler(
    AutofillOfferManager* offer_manager)
    : offer_manager_(*offer_manager) {}

OfferNotificationHandler::~OfferNotificationHandler() = default;

void OfferNotificationHandler::UpdateOfferNotificationVisibility(
    AutofillClient& client) {
  const GURL url = client.GetLastCommittedPrimaryMainFrameURL();

  if (ValidOfferExistsForUrl(url)) {
    // TODO(crbug.com/40179715): GetOfferForUrl needs to know whether to give
    //   precedence to card-linked offers or promo code offers. Eventually,
    //   promo code offers should take precedence if a bubble is shown.
    //   Currently, if a url has both types of offers and the promo code offer
    //   is selected, no bubble will end up being shown (due to not yet being
    //   implemented).
    AutofillOfferData* offer = offer_manager_->GetOfferForUrl(url);
    CHECK(IsOfferValid(offer));
    int64_t offer_id = offer->GetOfferId();
    bool offer_id_has_shown_before = shown_notification_ids_.contains(offer_id);
    client.GetPaymentsAutofillClient()->UpdateOfferNotification(
        *offer,
        {.notification_has_been_shown = offer_id_has_shown_before,
         .show_notification_automatically = !offer_id_has_shown_before});
    shown_notification_ids_.insert(offer_id);
  } else {
    client.GetPaymentsAutofillClient()->DismissOfferNotification();
  }
}

void OfferNotificationHandler::ClearShownNotificationIdForTesting() {
  shown_notification_ids_.clear();
}

void OfferNotificationHandler::AddShownNotificationIdForTesting(
    int64_t shown_notification_id) {
  shown_notification_ids_.insert(shown_notification_id);
}

bool OfferNotificationHandler::ValidOfferExistsForUrl(const GURL& url) {
  return offer_manager_->IsUrlEligible(url) &&
         IsOfferValid(offer_manager_->GetOfferForUrl(url));
}

}  // namespace autofill
