// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace autofill {

class AutofillOfferManager;
class ContentAutofillClient;

// Shows or dismisses the offer notification on a tab. The notification is
// updated when the tab navigates, when it becomes visible and when the payments
// data changes. There is one per tab, owned by the tab's
// `ChromePaymentsAutofillClient`. It is only created if the tab's browser
// context has an `AutofillOfferManager`, so there is none in off-the-record
// tabs, e.g. in Incognito windows.
class OfferNotificationHandler : public content::WebContentsObserver,
                                 public PaymentsDataManager::Observer {
 public:
  explicit OfferNotificationHandler(ContentAutofillClient& client);
  OfferNotificationHandler(const OfferNotificationHandler&) = delete;
  OfferNotificationHandler& operator=(const OfferNotificationHandler&) = delete;
  ~OfferNotificationHandler() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // PaymentsDataManager::Observer:
  void OnPaymentsDataChanged() override;

 protected:
  // Returns whether the offer notification bubble is showing on the tab.
  // Virtual for testing.
  virtual bool IsOfferNotificationBubbleShowing() const;

 private:
  // Dismisses or updates the offer notification for the current page.
  void UpdateOfferNotificationVisibility();

  bool ValidOfferExistsForUrl(const GURL& url) const;

  // Returns the browser context's offer manager. The handler is only created
  // if there is one.
  AutofillOfferManager& GetOfferManager() const;

  const raw_ref<ContentAutofillClient> client_;

  base::ScopedObservation<PaymentsDataManager, PaymentsDataManager::Observer>
      payments_data_manager_observation_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HANDLER_H_
