// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_infobar_controller_impl.h"

#include "chrome/browser/ui/android/infobars/autofill_offer_notification_infobar.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_helper.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "ui/android/window_android.h"

namespace autofill {

OfferNotificationInfoBarControllerImpl::OfferNotificationInfoBarControllerImpl(
    content::WebContents* contents)
    : web_contents_(contents) {}

void OfferNotificationInfoBarControllerImpl::ShowIfNecessary(
    const AutofillOfferData* offer,
    const CreditCard* card) {
  DCHECK(offer);

  OfferNotificationHelper::CreateForWebContents(web_contents_);
  OfferNotificationHelper* offer_notification_helper =
      OfferNotificationHelper::FromWebContents(web_contents_);
  if (offer_notification_helper->OfferNotificationHasAlreadyBeenShown())
    return;

  std::vector<GURL> origins_to_display_infobar = offer->merchant_origins;
  if (card) {
    infobars::ContentInfoBarManager::FromWebContents(web_contents_)
        ->AddInfoBar(std::make_unique<AutofillOfferNotificationInfoBar>(
            std::make_unique<AutofillOfferNotificationInfoBarDelegateMobile>(
                offer->offer_details_url, *card)));
    offer_notification_helper->OnDisplayOfferNotification(
        origins_to_display_infobar);
  }
}

}  // namespace autofill
