// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;
class AutofillOfferManager;

// The class to handle actions related to the offer notifications. It is owned
// by the AutofillOfferManager and it is one per browser context.
class OfferNotificationHandler {
 public:
  explicit OfferNotificationHandler(AutofillOfferManager* offer_manager);
  OfferNotificationHandler(const OfferNotificationHandler&) = delete;
  OfferNotificationHandler& operator=(const OfferNotificationHandler&) = delete;
  ~OfferNotificationHandler();

  // Dismisses or updates the offer notification.
  void UpdateOfferNotificationVisibility(AutofillClient& client);

 private:
  bool ValidOfferExistsForUrl(const GURL& url);

  // The reference to the offer manager that owns |this|.
  raw_ref<AutofillOfferManager> offer_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_
