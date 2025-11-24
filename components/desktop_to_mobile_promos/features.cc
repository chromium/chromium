// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desktop_to_mobile_promos/features.h"

#include "build/build_config.h"
#include "components/sync_preferences/features.h"

BASE_FEATURE(kMobilePromoOnDesktop, base::FEATURE_DISABLED_BY_DEFAULT);

const char kMobilePromoOnDesktopPromoTypeParam[] =
    "mobile_promo_on_desktop_promo_type";
const char kMobilePromoOnDesktopNotificationParam[] =
    "mobile_promo_on_desktop_notification";

bool MobilePromoOnDesktopEnabled() {
  return base::FeatureList::IsEnabled(
             sync_preferences::features::kEnableCrossDevicePrefTracker) &&
         base::FeatureList::IsEnabled(kMobilePromoOnDesktop);
}

bool MobilePromoOnDesktopTypeEnabled(MobilePromoOnDesktopPromoType type) {
  if (!MobilePromoOnDesktopEnabled()) {
    return false;
  }

  auto enabled_promo_type = static_cast<MobilePromoOnDesktopPromoType>(
      base::GetFieldTrialParamByFeatureAsInt(
          kMobilePromoOnDesktop, kMobilePromoOnDesktopPromoTypeParam,
          static_cast<int>(MobilePromoOnDesktopPromoType::kAllPromos)));

  if (enabled_promo_type == MobilePromoOnDesktopPromoType::kAllPromos) {
    return true;
  }

  return enabled_promo_type == type;
}

bool IsMobilePromoOnDesktopNotificationsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMobilePromoOnDesktop, kMobilePromoOnDesktopNotificationParam, false);
}
