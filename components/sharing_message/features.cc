// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/features.h"

#include "build/build_config.h"
#include "components/sync_preferences/features.h"

BASE_FEATURE(kClickToCall, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktop, base::FEATURE_DISABLED_BY_DEFAULT);

const char kMobilePromoOnDesktopPromoTypeParam[] =
    "mobile_promo_on_desktop_promo_type";
const char kMobilePromoOnDesktopNotificationParam[] =
    "mobile_promo_on_desktop_notification";

MobilePromoOnDesktopPromoType MobilePromoOnDesktopTypeEnabled() {
  if (!base::FeatureList::IsEnabled(
          sync_preferences::features::kEnableCrossDevicePrefTracker) ||
      !base::FeatureList::IsEnabled(kMobilePromoOnDesktop)) {
    return MobilePromoOnDesktopPromoType::kDisabled;
  }
  return static_cast<MobilePromoOnDesktopPromoType>(
      base::GetFieldTrialParamByFeatureAsInt(
          kMobilePromoOnDesktop, kMobilePromoOnDesktopPromoTypeParam, 1));
}

bool IsMobilePromoOnDesktopNotificationsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMobilePromoOnDesktop, kMobilePromoOnDesktopNotificationParam, false);
}
