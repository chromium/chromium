// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_INFOBAR_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_INFOBAR_CONTROLLER_IMPL_H_

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Per-tab controller to control the offer notification infobar displayed on
// mobile.
class OfferNotificationInfoBarControllerImpl {
 public:
  explicit OfferNotificationInfoBarControllerImpl(
      content::WebContents* contents);
  ~OfferNotificationInfoBarControllerImpl() = default;

  OfferNotificationInfoBarControllerImpl(
      const OfferNotificationInfoBarControllerImpl&) = delete;
  OfferNotificationInfoBarControllerImpl& operator=(
      const OfferNotificationInfoBarControllerImpl&) = delete;

  // Show the infobar unless it was already shown in the same tab with the same
  // origin.
  void ShowIfNecessary(const std::vector<GURL>& origins_to_display_infobar,
                       const GURL& offer_details_url,
                       const CreditCard* card);

 private:
  content::WebContents* web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_INFOBAR_CONTROLLER_IMPL_H_
