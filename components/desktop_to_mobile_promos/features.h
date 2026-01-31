// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/desktop_to_mobile_promos/promos_types.h"

// Enum to represent promo types of feature kMobilePromoOnDesktop.
enum class MobilePromoOnDesktopPromoType {
  kAllPromos = 0,
  kLensPromo = 1,
  kESBPromo = 2,
  kAutofillPromo = 3,
  kTabGroups = 4,
  kPriceTracking = 5,
};

// Enum to represent the forced promo type of feature
// kMobilePromoOnDesktopForcePromoType.
enum class IOSPromoBubbleForceType {
  kQRCode = 0,
  kReminder = 1,
  kNoOverride = 2,
};

// If this feature is enabled, show mobile promo on desktop with a "Remind Me"
// button.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopWithReminder);

// If this feature is enabled, collect data for the mobile promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopRecordActiveDays);

// If this feature is enabled, show the QR Code flow for the mobile
// promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopWithQRCode);

// If this feature is enabled, force the iOS promo to be a specific type.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopForcePromoType);

// Parameter of `kMobilePromoOnDesktop` for promo type.
extern const char kMobilePromoOnDesktopPromoTypeParam[];
// Parameter of `kMobilePromoOnDesktop` for showing the iOS push notification.
extern const char kMobilePromoOnDesktopNotificationParam[];

// Parameter of `kMobilePromoOnDesktopForcePromoType` for the promo type.
extern const char kMobilePromoOnDesktopForcePromoTypeParam[];

// Returns true if either the `kMobilePromoOnDesktopWithReminder` or
// `kMobilePromoOnDesktopWithQRCode` feature is enabled.
bool MobilePromoOnDesktopEnabled();

// Returns true if the `kMobilePromoOnDesktopRecordActiveDays` feature is
// enabled.
bool IsMobilePromoOnDesktopRecordActiveDaysEnabled();

// Returns whether the given promo type is enabled for feature
// `kMobilePromoOnDesktop` or `kMobilePromoOnDesktopWithQRCode`. If the promo
// type parameter for the feature is not set, this will return true for any
// promo type.
bool MobilePromoOnDesktopTypeEnabled(MobilePromoOnDesktopPromoType type);

// Returns whether the given promo type is enabled for the feature corresponding
// to the provided `bubble_type` (`kMobilePromoOnDesktopWithReminder` for
// Reminder or `kMobilePromoOnDesktopWithQRCode` for QRCode).
bool MobilePromoOnDesktopTypeEnabled(
    MobilePromoOnDesktopPromoType type,
    desktop_to_mobile_promos::BubbleType bubble_type);

// Returns true if feature `kMobilePromoOnDesktopWithReminder` is enabled with a
// push notification arm, false otherwise.
bool IsMobilePromoOnDesktopNotificationsEnabled();

// Returns the forced promo type if `kMobilePromoOnDesktopForcePromoType` is
// enabled, otherwise returns `IOSPromoBubbleType::kNoOverride`.
IOSPromoBubbleForceType GetMobilePromoOnDesktopForcePromoType();

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
