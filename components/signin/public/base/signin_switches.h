// Copyright 2014 The Chromium Authors. All rights reserved.
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
extern const char kClearTokenService[];
extern const char kDisableSigninScopedDeviceId[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const base::Feature kAccountIdMigration;
#endif

#if defined(OS_ANDROID)
// This feature flag is for the deprecating of the Android profile data
// Menagerie API.
extern const base::Feature kDeprecateMenagerieAPI;
// This feature flag is used to wipe device data on child account signin.
extern const base::Feature kWipeDataOnChildAccountSignin;
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_IOS)
// Features to trigger the startup sign-in promo at boot.
extern const base::Feature kForceStartupSigninPromo;
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Support for the minor mode.
extern const base::Feature kMinorModeSupport;
#endif

// This feature disables all extended sync promos.
extern const base::Feature kForceDisableExtendedSyncPromos;

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
