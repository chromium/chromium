// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREF_NAMES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"

namespace prefs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kForceLogoutUnauthenticatedUserEnabled[];
extern const char kAccountIdMigrationState[];
#endif
extern const char kAccountInfo[];
extern const char kAutologinEnabled[];
extern const char kGaiaCookieHash[];
extern const char kGaiaCookieChangedTime[];
extern const char kGaiaCookiePeriodicReportTime[];
extern const char kGoogleServicesAccountId[];
extern const char kGoogleServicesConsentedToSync[];
extern const char kGoogleServicesLastAccountIdDeprecated[];
extern const char kGoogleServicesLastGaiaId[];
extern const char kGoogleServicesLastUsername[];
extern const char kGoogleServicesSigninScopedDeviceId[];
extern const char kGoogleServicesUsernamePattern[];
extern const char kRestrictAccountsToPatterns[];
extern const char kReverseAutologinRejectedEmailList[];
extern const char kSignedInWithCredentialProvider[];
extern const char kSigninAllowed[];
extern const char kGaiaCookieLastListAccountsData[];

}  // namespace prefs

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREF_NAMES_H_
