// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Enum to represent promo types of feature kMobilePromoOnDesktop.
enum class MobilePromoOnDesktopPromoType {
  kAllPromos = 0,
  kLensPromo = 1,
  kESBPromo = 2,
  kAutofillPromo = 3,
};

// If this feature is enabled, show mobile promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktop);

// Parameter of `kMobilePromoOnDesktop` for promo type.
extern const char kMobilePromoOnDesktopPromoTypeParam[];
// Parameter of `kMobilePromoOnDesktop` for showing the iOS push notification.
extern const char kMobilePromoOnDesktopNotificationParam[];

// Returns true if the `kMobilePromoOnDesktop` feature is enabled.
bool MobilePromoOnDesktopEnabled();

// Returns whether the given promo type is enabled for feature
// `kMobilePromoOnDesktop`. If the promo type parameter for the feature is not
// set, this will return true for any promo type.
bool MobilePromoOnDesktopTypeEnabled(MobilePromoOnDesktopPromoType type);

// Returns true if feature `kMobilePromoOnDesktop` is enabled with a push
// notification arm, false otherwise.
bool IsMobilePromoOnDesktopNotificationsEnabled();

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
