// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desktop_to_mobile_promos/features.h"

#include "build/build_config.h"
#include "components/sync_preferences/features.h"

BASE_FEATURE(kMobilePromoOnDesktop, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopRecordActiveDays,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopForcePromoType,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kMobilePromoOnDesktopPromoTypeParam[] =
    "mobile_promo_on_desktop_promo_type";
const char kMobilePromoOnDesktopNotificationParam[] =
    "mobile_promo_on_desktop_notification";

const char kMobilePromoOnDesktopForcePromoTypeParam[] =
    "mobile_promo_on_desktopforce_force_promo_type";

bool MobilePromoOnDesktopEnabled() {
  return base::FeatureList::IsEnabled(
             sync_preferences::features::kEnableCrossDevicePrefTracker) &&
         base::FeatureList::IsEnabled(kMobilePromoOnDesktop);
}

bool IsMobilePromoOnDesktopRecordActiveDaysEnabled() {
  if (!base::FeatureList::IsEnabled(
          sync_preferences::features::kEnableCrossDevicePrefTracker)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kMobilePromoOnDesktopRecordActiveDays);
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

IOSPromoBubbleForceType GetMobilePromoOnDesktopForcePromoType() {
  if (!base::FeatureList::IsEnabled(kMobilePromoOnDesktopForcePromoType)) {
    return IOSPromoBubbleForceType::kNoOverride;
  }

  return static_cast<IOSPromoBubbleForceType>(
      base::GetFieldTrialParamByFeatureAsInt(
          kMobilePromoOnDesktopForcePromoType,
          kMobilePromoOnDesktopForcePromoTypeParam,
          static_cast<int>(IOSPromoBubbleForceType::kReminder)));
}
