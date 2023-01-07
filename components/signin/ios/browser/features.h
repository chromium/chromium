// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_FEATURES_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace signin {

// Returns true if the startup sign-in promo should be displayed at boot.
bool ForceStartupSigninPromo();

// Name of multi-value switch that controls the delay (in minutes) for polling
// for the existence of Gaia cookies for google.com.
extern const char kDelayThresholdMinutesToUpdateGaiaCookie[];

// Name of multi-value switch that controls the max time (in seconds) for
// waiting for a response from the Account Capabilities API.
extern const char kWaitThresholdMillisecondsForCapabilitiesApi[];

// Feature to enable FRE MICe.
BASE_DECLARE_FEATURE(kNewMobileIdentityConsistencyFRE);

// Feature to enable Unicorn account sign-in for iOS.
BASE_DECLARE_FEATURE(kEnableUnicornAccountSupport);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_FEATURES_H_
