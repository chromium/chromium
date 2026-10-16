// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;
class AutofillOfferManager;

// Shows or dismisses the offer notification on a tab. There is one per tab,
// owned by the tab's `ChromePaymentsAutofillClient`. It is only created if the
// tab's browser context has an `AutofillOfferManager`, so there is none in
// off-the-record tabs, e.g. in Incognito windows.
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

  // The offer manager of the tab's browser context.
  raw_ref<AutofillOfferManager> offer_manager_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_
