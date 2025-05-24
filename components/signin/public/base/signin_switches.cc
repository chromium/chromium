// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCctSignInPrompt,
             "CctSignInPrompt",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Add history sync opt-in promo in the History Page.
BASE_FEATURE(kHistoryPageHistorySyncPromo,
             "HistoryPageHistorySyncPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes the History Page history opt-in promo use a different CTA String.
// No-op unless "HistoryPageHistorySyncPromo" is enabled.
BASE_FEATURE(kHistoryPagePromoCtaStringVariation,
             "HistoryPagePromoCtaStringVariation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a history sync educational tip in the magic stack on NTP.
BASE_FEATURE(kHistoryOptInEducationalTip,
             "HistoryOptInEducationalTip",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Determines which text should be shown on the history sync educational tip
// button. No-op unless HistoryOptInEducationalTip is enabled.
const base::FeatureParam<int> kHistoryOptInEducationalTipVariation(
    &kHistoryOptInEducationalTip,
    "history_opt_in_educational_tip_param",
    0);

// Feature to bypass double-checking that signin callers have correctly gotten
// the user to accept account management. This check is slow and not strictly
// necessary, so disable it while we work on adding caching.
// TODO(https://crbug.com/339457762): Restore the check when we implement
// caching.
BASE_FEATURE(kSkipCheckForAccountManagementOnSignin,
             "SkipCheckForAccountManagementOnSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnoForAuto, "UnoForAuto", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseHostedDomainForManagementCheckOnSignin,
             "UseHostedDomainForManagementCheckOnSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMakeAccountsAvailableInIdentityManager,
             "MakeAccountsAvailableInIdentityManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenSignInPromoUseDate,
             "FullscreenSignInPromoUseDate",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables the History Sync Opt-in on Desktop.
BASE_FEATURE(kEnableHistorySyncOptin,
             "EnableHistorySyncOptin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables the History Sync Opt-in expansion pill on Desktop.
BASE_FEATURE(kEnableHistorySyncOptinExpansionPill,
             "EnableHistorySyncOptinExpansionPill",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<HistorySyncOptinExpansionPillOption>::Option
    kHistorySyncOptinExpansionPillOptions[] = {
        {HistorySyncOptinExpansionPillOption::kBrowseAcrossDevices,
         "browse-across-devices"},
        {HistorySyncOptinExpansionPillOption::kSyncHistory, "sync-history"},
        {HistorySyncOptinExpansionPillOption::kSeeTabsFromOtherDevices,
         "see-tabs-from-other-devices"},
        {HistorySyncOptinExpansionPillOption::
             kBrowseAcrossDevicesNewProfileMenuPromoVariant,
         "browse-across-devices-new-profile-menu-promo-variant"}};

// Determines the experiment arm of the History Sync Opt-in expansion pill
// (different text options for the pill and the profile menu promo variant).
//
// It is no-op unless "EnableHistorySyncOptin" is enabled.
constexpr base::FeatureParam<HistorySyncOptinExpansionPillOption>
    kHistorySyncOptinExpansionPillOption = {
        &kEnableHistorySyncOptinExpansionPill,
        "history-sync-optin-expansion-pill-option",
        HistorySyncOptinExpansionPillOption::kBrowseAcrossDevices,
        &kHistorySyncOptinExpansionPillOptions};

// Force enable the default browser step in the first run experience on Desktop.
const char kForceFreDefaultBrowserStep[] = "force-fre-default-browser-step";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Enable experimental binding session credentials to the device.
BASE_FEATURE(kEnableBoundSessionCredentials,
             "EnableBoundSessionCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBoundSessionCredentialsEnabled(const PrefService* profile_prefs) {
  // Enterprise policy takes precedence over the feature value.
  if (profile_prefs->HasPrefPath(prefs::kBoundSessionCredentialsEnabled)) {
    return profile_prefs->GetBoolean(prefs::kBoundSessionCredentialsEnabled);
  }

  return base::FeatureList::IsEnabled(kEnableBoundSessionCredentials);
}

const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>::Option
    enable_bound_session_credentials_dice_support[] = {
        {EnableBoundSessionCredentialsDiceSupport::kDisabled, "disabled"},
        {EnableBoundSessionCredentialsDiceSupport::kEnabled, "enabled"}};
const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>
    kEnableBoundSessionCredentialsDiceSupport{
        &kEnableBoundSessionCredentials, "dice-support",
        EnableBoundSessionCredentialsDiceSupport::kEnabled,
        &enable_bound_session_credentials_dice_support};

// Restricts the DBSC registration URL path to a single allowed string.
// Set to "/" to denote an empty path.
// Set to an empty string to remove the restriction.
const base::FeatureParam<std::string>
    kEnableBoundSessionCredentialsExclusiveRegistrationPath{
        &kEnableBoundSessionCredentials, "exclusive-registration-path", ""};

// Enables Chrome refresh tokens binding to a device.
BASE_FEATURE(kEnableChromeRefreshTokenBinding,
             "EnableChromeRefreshTokenBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeRefreshTokenBindingEnabled(const PrefService* profile_prefs) {
  // Enterprise policy takes precedence over the feature value.
  if (profile_prefs->HasPrefPath(prefs::kBoundSessionCredentialsEnabled)) {
    return profile_prefs->GetBoolean(prefs::kBoundSessionCredentialsEnabled);
  }

  return base::FeatureList::IsEnabled(kEnableChromeRefreshTokenBinding);
}

// Allows to disable the bound session credentials code in case of emergency.
BASE_FEATURE(kBoundSessionCredentialsKillSwitch,
             "BoundSessionCredentialsKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will always use the /IssueToken endpoint to fetch access
// tokens, no matter if a refresh token is bound or not.
BASE_FEATURE(kUseIssueTokenToFetchAccessTokens,
             "UseIssueTokenToFetchAccessTokens",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kEnablePreferencesAccountStorage,
             "EnablePreferencesAccountStorage",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
BASE_FEATURE(kForceStartupSigninPromo,
             "ForceStartupSigninPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kInterceptBubblesDismissibleByAvatarButton,
             "InterceptBubblesDismissibleByAvatarButton",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableSnackbarInSettings,
             "EnableSnackbarInSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableImprovedGuestProfileMenu,
             "EnableImprovedGuestProfileMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePendingModePasswordsPromo,
             "EnablePendingModePasswordsPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)

BASE_FEATURE(kEnableIdentityInAuthError,
             "EnableIdentityInAuthError",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableErrorBadgeOnIdentityDisc,
             "EnableErrorBadgeOnIdentityDisc",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableASWebAuthenticationSession,
             "EnableASWebAuthenticationSession",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables showing the enterprise dialog after every signin into a managed
// account.
BASE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin,
             "ShowEnterpriseDialogForAllManagedAccountsSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExtensionsExplicitBrowserSignin,
             "EnableExtensionsExplicitBrowserSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsExtensionsExplicitBrowserSigninEnabled() {
  return base::FeatureList::IsEnabled(kEnableExtensionsExplicitBrowserSignin);
}

BASE_FEATURE(kSyncEnableBookmarksInTransportMode,
             "SyncEnableBookmarksInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_IOS)
);

BASE_FEATURE(kDeferWebSigninTrackerCreation,
             "DeferWebSigninTrackerCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace switches

#if BUILDFLAG(IS_CHROMEOS)
// Enables the generation of pseudo-stable per-user per-device device
// identifiers. This identifier can be reset by the user by powerwashing the
// device.
BASE_FEATURE(kStableDeviceId,
             "StableDeviceId",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kProfilesReordering,
             "ProfilesReordering",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kIgnoreMirrorHeadersInBackgoundTabs,
             "IgnoreMirrorHeadersInBackgoundTabs",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kNonDefaultGaiaOriginCheck,
             "NonDefaultGaiaOriginCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);
