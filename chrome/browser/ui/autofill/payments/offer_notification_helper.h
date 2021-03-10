// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HELPER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Tracks navigations in a tab to determine whether offer notifications have or
// should be shown.
class OfferNotificationHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OfferNotificationHelper> {
 public:
  ~OfferNotificationHelper() override;

  // Returns true if the offer notification has been displayed for the current
  // site.
  bool OfferNotificationHasAlreadyBeenShown();

  // Notifies the helper that the offer notification was displayed for the given
  // set of URLs.
  void OnDisplayOfferNotification(const std::vector<GURL>& origins);

  // WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 protected:
  explicit OfferNotificationHelper(content::WebContents* contents);

 private:
  friend class content::WebContentsUserData<OfferNotificationHelper>;

  // The set of origins per-tab that are eligible to show the offer
  // notification. This is populated when OnDisplayOfferNotification() is
  // called and is cleared when navigating to origins outside of this set.
  std::vector<GURL> origins_to_display_notification_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_HELPER_H_
