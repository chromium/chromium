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
#else
extern const base::Feature kForceAccountIdMigration;
#endif

#if defined(OS_ANDROID)
// This feature flag is for the deprecating of the Android profile data
// Menagerie API.
extern const base::Feature kDeprecateMenagerieAPI;
#endif  // defined(OS_ANDROID)
}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
