// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desktop_to_mobile_promos/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/sync_preferences/features.h"

BASE_FEATURE(kMobilePromoOnDesktopWithReminder,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopWithReminderWave1,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopRecordActiveDays,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopWithQRCode,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopWithQRCodeWave1,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobilePromoOnDesktopForcePromoType,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMobileNTPPromoOnDesktop, base::FEATURE_DISABLED_BY_DEFAULT);

const char kMobilePromoOnDesktopPromoTypeParam[] =
    "mobile_promo_on_desktop_promo_type";
const char kMobilePromoOnDesktopNotificationParam[] =
    "mobile_promo_on_desktop_notification";

const char kMobilePromoOnDesktopForcePromoTypeParam[] =
    "mobile_promo_on_desktopforce_force_promo_type";

const char kMobileNTPPromoOnDesktopVariationParam[] =
    "mobile_ntp_promo_on_desktop_promo_type";

bool MobilePromoOnDesktopEnabled() {
  return base::FeatureList::IsEnabled(
             sync_preferences::features::kEnableCrossDevicePrefTracker) &&
         (base::FeatureList::IsEnabled(kMobilePromoOnDesktopWithReminder) ||
          base::FeatureList::IsEnabled(kMobilePromoOnDesktopWithQRCode) ||
          base::FeatureList::IsEnabled(kMobilePromoOnDesktopWithQRCodeWave1) ||
          base::FeatureList::IsEnabled(kMobilePromoOnDesktopWithReminderWave1));
}

bool IsMobilePromoOnDesktopRecordActiveDaysEnabled() {
  return base::FeatureList::IsEnabled(kMobilePromoOnDesktopRecordActiveDays);
}

bool MobilePromoOnDesktopTypeEnabled(MobilePromoOnDesktopPromoType type) {
  if (!MobilePromoOnDesktopEnabled()) {
    return false;
  }
  return MobilePromoOnDesktopTypeEnabled(
             type, desktop_to_mobile_promos::BubbleType::kQRCode) ||
         MobilePromoOnDesktopTypeEnabled(
             type, desktop_to_mobile_promos::BubbleType::kReminder);
}

bool MobilePromoOnDesktopTypeEnabled(
    MobilePromoOnDesktopPromoType type,
    desktop_to_mobile_promos::BubbleType bubble_type) {
  const base::Feature* feature = nullptr;
  bool is_qr_code =
      (bubble_type == desktop_to_mobile_promos::BubbleType::kQRCode);

  switch (type) {
    case MobilePromoOnDesktopPromoType::kESBPromo:
    case MobilePromoOnDesktopPromoType::kAutofillPromo:
    case MobilePromoOnDesktopPromoType::kAllPromos:
      feature = is_qr_code ? &kMobilePromoOnDesktopWithQRCode
                           : &kMobilePromoOnDesktopWithReminder;
      break;
    case MobilePromoOnDesktopPromoType::kLensPromo:
    case MobilePromoOnDesktopPromoType::kTabGroups:
    case MobilePromoOnDesktopPromoType::kPriceTracking:
      feature = is_qr_code ? &kMobilePromoOnDesktopWithQRCodeWave1
                           : &kMobilePromoOnDesktopWithReminderWave1;
      break;
    default:
      return false;
  }

  if (!base::FeatureList::IsEnabled(
          sync_preferences::features::kEnableCrossDevicePrefTracker) ||
      !base::FeatureList::IsEnabled(*feature)) {
    return false;
  }

  std::string param_value = base::GetFieldTrialParamValueByFeature(
      *feature, kMobilePromoOnDesktopPromoTypeParam);

  if (param_value.empty()) {
    // Default to kAllPromos if no param is set.
    return true;
  }

  // Check if any of the promo types in the comma separated list equate to
  // `type` and return true if so.
  std::vector<std::string> promo_types = base::SplitString(
      param_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& promo_type_str : promo_types) {
    int promo_type_int;
    if (base::StringToInt(promo_type_str, &promo_type_int)) {
      auto enabled_type =
          static_cast<MobilePromoOnDesktopPromoType>(promo_type_int);
      if (enabled_type == MobilePromoOnDesktopPromoType::kAllPromos ||
          enabled_type == type) {
        return true;
      }
    }
  }

  return false;
}

bool IsMobilePromoOnDesktopNotificationsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
             kMobilePromoOnDesktopWithReminder,
             kMobilePromoOnDesktopNotificationParam, false) ||
         base::GetFieldTrialParamByFeatureAsBool(
             kMobilePromoOnDesktopWithReminderWave1,
             kMobilePromoOnDesktopNotificationParam, false);
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

bool IsMobileNTPPromoOnDesktopEnabled() {
  return base::FeatureList::IsEnabled(kMobileNTPPromoOnDesktop);
}

bool IsMobileNTPPromoOnDesktopVariationEnabled(
    MobileNTPPromoOnDesktopVariation variation) {
  if (!IsMobileNTPPromoOnDesktopEnabled()) {
    return false;
  }

  int param_value = base::GetFieldTrialParamByFeatureAsInt(
      kMobileNTPPromoOnDesktop, kMobileNTPPromoOnDesktopVariationParam,
      static_cast<int>(MobileNTPPromoOnDesktopVariation::kAll));

  MobileNTPPromoOnDesktopVariation enabled_variation =
      static_cast<MobileNTPPromoOnDesktopVariation>(param_value);

  if (enabled_variation == MobileNTPPromoOnDesktopVariation::kAll) {
    return true;
  }

  return enabled_variation == variation;
}
