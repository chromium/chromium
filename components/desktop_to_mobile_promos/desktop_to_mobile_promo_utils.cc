// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desktop_to_mobile_promos/desktop_to_mobile_promo_utils.h"

#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"

namespace desktop_to_mobile_promos {

PromoNotificationStringIDs GetPromoNotificationStringIDs(PromoType promo_type) {
  PromoNotificationStringIDs ids;

  switch (promo_type) {
    case PromoType::kPassword:
      ids.title_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_NOTIFICATION_TITLE;
      ids.body_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_NOTIFICATION_BODY;
      break;
    case PromoType::kEnhancedBrowsing:
      ids.title_id = IDS_IOS_DESKTOP_SAFE_BROWSING_PROMO_NOTIFICATION_TITLE;
      ids.body_id = IDS_IOS_DESKTOP_SAFE_BROWSING_PROMO_NOTIFICATION_BODY;
      break;
    case PromoType::kLens:
      ids.title_id = IDS_IOS_DESKTOP_LENS_PROMO_NOTIFICATION_TITLE;
      ids.body_id = IDS_IOS_DESKTOP_LENS_PROMO_NOTIFICATION_BODY;
      break;
    case PromoType::kTabGroups:
      ids.title_id = IDS_IOS_DESKTOP_TAB_GROUPS_PROMO_NOTIFICATION_TITLE;
      ids.body_id = IDS_IOS_DESKTOP_TAB_GROUPS_PROMO_NOTIFICATION_BODY;
      break;
    case PromoType::kPriceTracking:
      ids.title_id = IDS_IOS_DESKTOP_PRICE_TRACKING_PROMO_NOTIFICATION_TITLE;
      ids.body_id = IDS_IOS_DESKTOP_PRICE_TRACKING_PROMO_NOTIFICATION_BODY;
      break;
    case PromoType::kAddress:
    case PromoType::kPayment:
      break;
  }

  return ids;
}

}  // namespace desktop_to_mobile_promos
