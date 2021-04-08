// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

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
#else
const base::Feature kForceAccountIdMigration{"ForceAccountIdMigration",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// This feature flag is for deprecating of the Android profile data
// Menagerie API.
const base::Feature kDeprecateMenagerieAPI{"DeprecateMenagerieAPI",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kUseAccountManagerFacade{"kUseAccountManagerFacade",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
const base::Feature kUseAccountManagerFacade{"kUseAccountManagerFacade",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif
}  // namespace switches
