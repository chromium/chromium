// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAccountIdMigration{"AccountIdMigration",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// If enabled, child accounts (i.e. Unicorn accounts) on Android do not have the
// Sync feature forced on.
const base::Feature kAllowSyncOffForChildAccounts{
    "AllowSyncOffForChildAccounts", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

// This feature disables all extended sync promos.
const base::Feature kForceDisableExtendedSyncPromos{
    "ForceDisableExtendedSyncPromos", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
const base::Feature kForceStartupSigninPromo{"ForceStartupSigninPromo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Allows local (not signed-in) profiles on lacros.
const base::Feature kLacrosNonSyncingProfiles{
    "LacrosNonSyncingProfiles", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace switches
