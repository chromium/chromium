// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace switches {

// These switches should not be queried from CommandLine::HasSwitch() directly.
// Always go through the helper functions in account_consistency_method.h
// to properly take into account the state of field trials.

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const base::Feature kAccountIdMigration;
#endif

#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kAllowSyncOffForChildAccounts;
extern const base::Feature kCreateSigninCheckerBeforeSyncConsentFragment;
#endif

extern const base::Feature kNewSigninRequestHeaderCheckOrder;

extern const char kClearTokenService[];

extern const char kDisableSigninScopedDeviceId[];

extern const base::Feature kEnableFetchingAccountCapabilities;

extern const base::Feature kForceDisableExtendedSyncPromos;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
extern const base::Feature kEnableCbdSignOut;
extern const base::Feature kForceStartupSigninPromo;
extern const base::Feature kTangibleSync;
#endif

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
