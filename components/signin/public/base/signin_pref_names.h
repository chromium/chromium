// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREF_NAMES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREF_NAMES_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"

namespace prefs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kForceLogoutUnauthenticatedUserEnabled[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kAccountIdMigrationState[];
#endif
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kAccountInfo[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kCookieClearOnExitMigrationNoticeComplete[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGaiaCookieHash[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGaiaCookieChangedTime[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGaiaCookiePeriodicReportTime[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesAccountId[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesConsentedToSync[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesLastSyncingGaiaId[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesLastSyncingUsername[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesLastSignedInUsername[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesSigninScopedDeviceId[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesSyncingGaiaIdMigratedToSignedIn[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesSyncingUsernameMigratedToSignedIn[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGoogleServicesUsernamePattern[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kHistorySyncLastDeclinedTimestamp[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kHistorySyncSuccessiveDeclineCount[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kRestrictAccountsToPatterns[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kSignedInWithCredentialProvider[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kSigninAllowed[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kGaiaCookieLastListAccountsData[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kSigninAllowedOnNextStartup[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kSigninInterceptionIDPCookiesUrl[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kProfileSeparationSettings[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kProfileSeparationDataMigrationSettings[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kProfileSeparationDomainExceptionList[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kUserCloudSigninPolicyResponseFromPolicyTestPage[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kExplicitBrowserSignin[];
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kBoundSessionCredentialsEnabled[];

}  // namespace prefs

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREF_NAMES_H_
