// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_FEATURES_H_
#define COMPONENTS_SHARING_MESSAGE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

BASE_DECLARE_FEATURE(kClickToCall);

// If this feature is enabled, show mobile promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktop);

// Parameter of `kMobilePromoOnDesktop` for promo type.
extern const char kMobilePromoOnDesktopPromoTypeParam[];
// Parameter of `kMobilePromoOnDesktop` for showing the iOS push notification.
extern const char kMobilePromoOnDesktopNotificationParam[];

#endif  // COMPONENTS_SHARING_MESSAGE_FEATURES_H_
