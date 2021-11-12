// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_helper.h"

#include "content/public/browser/navigation_handle.h"

namespace autofill {

OfferNotificationHelper::OfferNotificationHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<OfferNotificationHelper>(*contents) {}

OfferNotificationHelper::~OfferNotificationHelper() = default;

bool OfferNotificationHelper::OfferNotificationHasAlreadyBeenShown() {
  return !origins_to_display_notification_.empty();
}

void OfferNotificationHelper::OnDisplayOfferNotification(
    const std::vector<GURL>& origins) {
  origins_to_display_notification_ = origins;
}

void OfferNotificationHelper::PrimaryPageChanged(content::Page& page) {
  // Don't do anything if user is still on an eligible origin for this offer.
  if (base::ranges::count(origins_to_display_notification_,
                          page.GetMainDocument()
                              .GetLastCommittedURL()
                              .DeprecatedGetOriginAsURL())) {
    return;
  }

  // TODO(crbug.com/1093057): Introduce a callback and hide the offer
  // notification.
  origins_to_display_notification_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationHelper);

}  // namespace autofill
