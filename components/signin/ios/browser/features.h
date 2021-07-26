// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_FEATURES_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace signin {

// This feature simplify sign-out UI in the account table view.
extern const base::Feature kSimplifySignOutIOS;

// Returns true if the startup sign-in promo should be displayed at boot.
bool ForceStartupSigninPromo();

// Returns true if extended sync promos should be disabled unconditionally.
bool ForceDisableExtendedSyncPromos();

// Returns true if the Chrome client can read the extended sync promo
// capability.
bool ExtendedSyncPromosCapabilityEnabled();

// Feature controlling whether to restore GAIA cookies when the user explicitly
// requests to sign in to a Google service.
extern const base::Feature kRestoreGaiaCookiesOnUserAction;

// Name of multi-value switch that controls the delay (in minutes) for polling
// for the existence of Gaia cookies for google.com.
extern const char kDelayThresholdMinutesToUpdateGaiaCookie[];

// Name of multi-value switch that controls the max time (in seconds) for
// waiting for a response from the Account Capabilities API.
extern const char kWaitThresholdMillisecondsForCapabilitiesApi[];

// Feature controlling whether to use full username in sign-in notifications.
extern const base::Feature kSigninNotificationInfobarUsernameInTitle;

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_FEATURES_H_
