// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace switches {

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAccountIdMigration{"AccountIdMigration",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
const base::Feature kForceStartupSigninPromo{"ForceStartupSigninPromo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kForceDisableExtendedSyncPromos{
    "ForceDisableExtendedSyncPromos", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
