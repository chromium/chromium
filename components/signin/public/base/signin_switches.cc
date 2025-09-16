// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCctSignInPrompt, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableSeamlessSignin, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceHistoryOptInScreen, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a history sync educational tip in the magic stack on NTP.
BASE_FEATURE(kHistoryOptInEducationalTip, base::FEATURE_ENABLED_BY_DEFAULT);

// Determines which text should be shown on the history sync educational tip
// button. No-op unless HistoryOptInEducationalTip is enabled.
const base::FeatureParam<int> kHistoryOptInEducationalTipVariation(
    &kHistoryOptInEducationalTip,
    "history_opt_in_educational_tip_param",
    1);

// When enabled a new library is used to fetch accounts via
// AccountManagerAccountManagerDelegate
BASE_FEATURE(kMigrateAccountManagerDelegate, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to bypass double-checking that signin callers have correctly gotten
// the user to accept account management. This check is slow and not strictly
// necessary, so disable it while we work on adding caching.
// TODO(https://crbug.com/339457762): Restore the check when we implement
// caching.
BASE_FEATURE(kSkipCheckForAccountManagementOnSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnoForAuto, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseHostedDomainForManagementCheckOnSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMakeAccountsAvailableInIdentityManager,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSmartEmailLineBreaking, base::FEATURE_ENABLED_BY_DEFAULT);

// Killswitch for the support of AddSession in web sign-in flow.
BASE_FEATURE(kSupportWebSigninAddSession, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Move the step of browser Signin into the Sync header processing logic.
// This flag is meant to be used as a kill switch, as the feature starts enabled
// by default.
BASE_FEATURE(kBrowserSigninInSyncHeaderOnGaiaIntegration,
             base::FEATURE_ENABLED_BY_DEFAULT);
// Whether we re-try showing the signing in interception bubble if the Dice
// sync header does not arrive within a time window from the LST token.
// This flag is meant to be used as a kill switch, as the feature starts enabled
// by default.
BASE_FEATURE(kRetryInterceptionBubbleOnDiceSyncHeaderTimeout,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the History Sync Opt-in expansion pill on Desktop.
BASE_FEATURE(kEnableHistorySyncOptinExpansionPill,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force enable the default browser step in the first run experience on Desktop.
const char kForceFreDefaultBrowserStep[] = "force-fre-default-browser-step";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kWebSigninLeadsToImplicitlySignedInState,
             // THIS IS A TEST-ONLY FLAG AND SHOULD NEVER BE ENABLED.
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Enable experimental binding session credentials to the device.
BASE_FEATURE(kEnableBoundSessionCredentials,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

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

// Allows to disable the bound session credentials code in case of emergency.
BASE_FEATURE(kBoundSessionCredentialsKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables Chrome refresh tokens binding to a device.
BASE_FEATURE(kEnableChromeRefreshTokenBinding,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsChromeRefreshTokenBindingEnabled(const PrefService* profile_prefs) {
  // Enterprise policy takes precedence over the feature value.
  if (profile_prefs->HasPrefPath(prefs::kBoundSessionCredentialsEnabled)) {
    return profile_prefs->GetBoolean(prefs::kBoundSessionCredentialsEnabled);
  }

  return base::FeatureList::IsEnabled(kEnableChromeRefreshTokenBinding);
}

// When enabled, Chrome will always use the /IssueToken endpoint to fetch access
// tokens, no matter if a refresh token is bound or not.
BASE_FEATURE(kUseIssueTokenToFetchAccessTokens,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables binding the OAuthMultilogin cookies to a device.
BASE_FEATURE(kEnableOAuthMultiloginCookiesBinding,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

BASE_FEATURE(kEnablePreferencesAccountStorage,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
BASE_FEATURE(kForceStartupSigninPromo, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenSignInPromoUseDate, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kInterceptBubblesDismissibleByAvatarButton,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

BASE_FEATURE(kOfferMigrationToDiceUsers, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kOfferMigrationToDiceUsersMinDelay,
                   &kOfferMigrationToDiceUsers,
                   "offer_migration_to_dice_users_min_delay",
                   base::Seconds(30));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kOfferMigrationToDiceUsersMaxDelay,
                   &kOfferMigrationToDiceUsers,
                   "offer_migration_to_dice_users_max_delay",
                   base::Minutes(5));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kOfferMigrationToDiceUsersMinTimeBetweenDialogs,
                   &kOfferMigrationToDiceUsers,
                   "offer_migration_to_dice_users_min_time_between_dialogs",
                   base::Days(7));

BASE_FEATURE(kRollbackDiceMigration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForcedDiceMigration, base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_IOS)

BASE_FEATURE(kEnableIdentityInAuthError, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableErrorBadgeOnIdentityDisc,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableASWebAuthenticationSession,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowlistScopesForMdmErrors, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSupportAddSessionEmailPrefill, base::FEATURE_ENABLED_BY_DEFAULT);

#endif

BASE_FEATURE(kEnableExtensionsExplicitBrowserSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsExtensionsExplicitBrowserSigninEnabled() {
  return base::FeatureList::IsEnabled(kEnableExtensionsExplicitBrowserSignin);
}

BASE_FEATURE(kSyncEnableBookmarksInTransportMode,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSkipRefreshTokenCheckInIdentityManager,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kSignInPromoMaterialNextUI, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyAddressBubbleSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyDiceWebSigninAccepted,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyDiceWebSigninDeclined,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyFirstRunSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyPasswordBubbleSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyProfileMenuDismissed,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyProfileMenuSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyProfilePickerAddProfileSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveySigninInterceptProfileSeparation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveySigninPromoBubbleDismissed,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfileMenu,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfilePicker,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeIdentitySurveyLaunchWithDelay,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kChromeIdentitySurveyLaunchWithDelayDuration,
                   &kChromeIdentitySurveyLaunchWithDelay,
                   "launch_delay_duration",
                   base::Milliseconds(3000));

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables the management disclaimer for managed signed profiles. All signed in
// profiles that never saw the management disclaimer will be shown the
// management disclaimer when they open Chrome. Every time the primary signed in
// account changes to a managed account, the management disclaimer will be
// shown. This is only for desktop platforms.
BASE_FEATURE(kEnforceManagementDisclaimer, base::FEATURE_DISABLED_BY_DEFAULT);

// The delay between policy registration retry.
const base::FeatureParam<base::TimeDelta> kPolicyRegistrationRetryDelay{
    &kEnforceManagementDisclaimer, "policy_registration_retry_delay",
    base::Hours(8)};
#endif

#if BUILDFLAG(IS_WIN)
// Enables expanding the Avatar Pill to show a sync promo. Expected to be used
// by Windows users only.
BASE_FEATURE(kAvatarButtonSyncPromo, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAvatarButtonSyncPromoMinimumCookieAgeParam,
                   &kAvatarButtonSyncPromo,
                   "minimum-cookie-age",
                   base::Days(14));
#endif
// Convenient testing flag for `kAvatarButtonSyncPromo` on all platforms.
// Also reduces the minimum cookie age to 30 seconds.
BASE_FEATURE(kAvatarButtonSyncPromoForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAvatarSyncPromoFeatureEnabled() {
  if (base::FeatureList::IsEnabled(
          switches::kAvatarButtonSyncPromoForTesting)) {
    return true;
  }
#if BUILDFLAG(IS_WIN)
  return (base::win::GetVersion() >= base::win::Version::WIN7 &&
          base::win::GetVersion() <= base::win::Version::WIN10_22H2) &&
         base::FeatureList::IsEnabled(switches::kAvatarButtonSyncPromo);
#else
  return false;
#endif
}

base::TimeDelta GetAvatarSyncPromoFeatureMinimumCookeAgeParam() {
  CHECK(IsAvatarSyncPromoFeatureEnabled());

  if (base::FeatureList::IsEnabled(
          switches::kAvatarButtonSyncPromoForTesting)) {
    return base::Seconds(30);
  }
#if BUILDFLAG(IS_WIN)
  return kAvatarButtonSyncPromoMinimumCookieAgeParam.Get();
#else
  NOTREACHED();
#endif
}

#if BUILDFLAG(IS_CHROMEOS)
// Enables the generation of pseudo-stable per-user per-device device
// identifiers. This identifier can be reset by the user by powerwashing the
// device.
BASE_FEATURE(kStableDeviceId, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kProfilesReordering, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kIgnoreMirrorHeadersInBackgoundTabs,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kNonDefaultGaiaOriginCheck, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace switches
