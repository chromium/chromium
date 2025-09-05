// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_FEATURES_H_
#define COMPONENTS_SHARING_MESSAGE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Enum to represent promo types of feature kMobilePromoOnDesktop.
enum class MobilePromoOnDesktopPromoType {
  kDisabled,
  kLensPromo,
  kESBPromo,
  kAutofillPromo,
};

BASE_DECLARE_FEATURE(kClickToCall);

// If this feature is enabled, show mobile promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktop);

// Parameter of `kMobilePromoOnDesktop` for promo type.
extern const char kMobilePromoOnDesktopPromoTypeParam[];
// Parameter of `kMobilePromoOnDesktop` for showing the iOS push notification.
extern const char kMobilePromoOnDesktopNotificationParam[];

// Returns which promo type is enabled for feature `kMobilePromoOnDesktop` or
// `kDisabled` if the feature is disabled.
MobilePromoOnDesktopPromoType MobilePromoOnDesktopTypeEnabled();

// Returns true if feature `kMobilePromoOnDesktop` is enabled with a push
// notification arm, false otherwise.
bool IsMobilePromoOnDesktopNotificationsEnabled();

#endif  // COMPONENTS_SHARING_MESSAGE_FEATURES_H_
