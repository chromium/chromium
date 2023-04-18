// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"
#include "base/feature_list.h"

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
// If enabled, starts gaia id fetching process from android accounts in
// AccountManagerFacade (AMF). Thus clients can get gaia id from AMF directly.
BASE_FEATURE(kGaiaIdCacheInAccountManagerFacade,
             "GaiaIdCacheInAccountManagerFacade",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIdentityStatusConsistency,
             "IdentityStatusConsistency",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kIdentityStatusConsistency,
             "IdentityStatusConsistency",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

// Enables fetching account capabilities and populating AccountInfo with the
// fetch result.
BASE_FEATURE(kEnableFetchingAccountCapabilities,
             "EnableFetchingAccountCapabilities",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature disables all extended sync promos.
BASE_FEATURE(kForceDisableExtendedSyncPromos,
             "ForceDisableExtendedSyncPromos",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
BASE_FEATURE(kForceStartupSigninPromo,
             "ForceStartupSigninPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables a new version of the sync confirmation UI.
BASE_FEATURE(kTangibleSync,
             "TangibleSync",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             // Fully rolled out on desktop: crbug.com/1430054
             base::FEATURE_ENABLED_BY_DEFAULT
#endif

);

}  // namespace switches
