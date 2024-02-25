// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
// Feature to refactor how and when accounts are seeded on Android.
BASE_FEATURE(kSeedAccountsRevamp,
             "SeedAccountsRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to apply enterprise policies on signin regardless of sync status.
BASE_FEATURE(kEnterprisePolicyOnSignin,
             "EnterprisePolicyOnSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Enable experimental binding session credentials to the device.
BASE_FEATURE(kEnableBoundSessionCredentials,
             "EnableBoundSessionCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBoundSessionCredentialsEnabled() {
  return base::FeatureList::IsEnabled(kEnableBoundSessionCredentials);
}

const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>::Option
    enable_bound_session_credentials_dice_support[] = {
        {EnableBoundSessionCredentialsDiceSupport::kDisabled, "disabled"},
        {EnableBoundSessionCredentialsDiceSupport::kEnabled, "enabled"}};
const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>
    kEnableBoundSessionCredentialsDiceSupport{
        &kEnableBoundSessionCredentials, "dice-support",
        EnableBoundSessionCredentialsDiceSupport::kDisabled,
        &enable_bound_session_credentials_dice_support};

// Restricts the DBSC registration URL path to a single allowed string.
// Set to "/" to denote an empty path.
// Set to an empty string to remove the restriction.
const base::FeatureParam<std::string>
    kEnableBoundSessionCredentialsExclusiveRegistrationPath{
        &kEnableBoundSessionCredentials, "exclusive-registration-path",
        "/RegisterSession"};

// Enables Chrome refresh tokens binding to a device. Requires
// "EnableBoundSessionCredentials" being enabled as a prerequisite.
BASE_FEATURE(kEnableChromeRefreshTokenBinding,
             "EnableChromeRefreshTokenBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeRefreshTokenBindingEnabled() {
  return IsBoundSessionCredentialsEnabled() &&
         base::FeatureList::IsEnabled(kEnableChromeRefreshTokenBinding);
}
#endif

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

#if BUILDFLAG(IS_ANDROID)
// Flag guarding the restoration of the signed-in only account instead of
// the syncing one and the restoration of account settings after device
// restore.
BASE_FEATURE(kRestoreSignedInAccountAndSettingsFromBackup,
             "RestoreSignedInAccountAndSettingsFromBackup",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables the search engine choice feature for existing users.
// TODO(b/316859558): Not used for shipping purposes, remove this feature.
BASE_FEATURE(kSearchEngineChoice,
             "SearchEngineChoice",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kUnoDesktop, "UnoDesktop", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kExplicitBrowserSigninUIOnDesktop,
             "ExplicitBrowserSigninUIOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kInterceptBubblesDismissibleByAvatarButton{
    &kExplicitBrowserSigninUIOnDesktop,
    /*name=*/"bubble_dismissible_by_avatar_button",
    /*default_value=*/true};

bool IsExplicitBrowserSigninUIOnDesktopEnabled(
    ExplicitBrowserSigninPhase phase) {
  if (phase == ExplicitBrowserSigninPhase::kFull) {
    return base::FeatureList::IsEnabled(kExplicitBrowserSigninUIOnDesktop);
  }
  return base::FeatureList::IsEnabled(kExplicitBrowserSigninUIOnDesktop) ||
         base::FeatureList::IsEnabled(kUnoDesktop);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kMinorModeRestrictionsForHistorySyncOptIn,
             "MinorModeRestrictionsForHistorySyncOptIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr int kMinorModeRestrictionsFetchDeadlineDefaultValueMs =
#if BUILDFLAG(IS_ANDROID)
    // Based on Signin.AccountCapabilities.UserVisibleLatency
    400;
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    // Based on Signin.SyncOptIn.PreSyncConfirmationLatency
    900;
#elif BUILDFLAG(IS_IOS)
    // Based on Signin.AccountCapabilities.UserVisibleLatency
    1000;
#endif

const base::FeatureParam<int> kMinorModeRestrictionsFetchDeadlineMs{
    &kMinorModeRestrictionsForHistorySyncOptIn,
    /*name=*/"MinorModeRestrictionsFetchDeadlineMs",
    kMinorModeRestrictionsFetchDeadlineDefaultValueMs};
#endif

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kRemoveSignedInAccountsDialog,
             "RemoveSignedInAccountsDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace switches
