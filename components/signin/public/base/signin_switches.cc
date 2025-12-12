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

// Command line switches, sorted by name.

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Force enable the default browser step in the first run experience on Desktop.
const char kForceFreDefaultBrowserStep[] = "force-fre-default-browser-step";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Feature declarations, sorted by the name of the BASE_FEATURE in each block.
// Please keep all FeatureParam and helper function definitions for a given
// feature in the same block as the feature definition.
//
// clang-format off
// keep-sorted start allow_yaml_lists=yes case=no group_prefixes=["#if", "#else", "#endif", "const", "};", "}", ");", "//", "bool", "base::", "BASE_FEATURE", "BASE_FEATURE_PARAM"] by_regex=["BASE_FEATURE\\(.*\\);"] skip_lines=2
// clang-format on

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kAccountRetrievalWaitsForRestoration,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAllowlistScopesForMdmErrors, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// Enables expanding the Avatar Pill to show a sync promo. Expected to be used
// by Windows users only.
BASE_FEATURE(kAvatarButtonSyncPromo, base::FEATURE_ENABLED_BY_DEFAULT);
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Allows to disable the bound session credentials code in case of emergency.
BASE_FEATURE(kBoundSessionCredentialsKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kCacheIdentityListInChrome, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableACPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCctSignInPrompt, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kChromeAndroidIdentitySurveyFirstRun,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeAndroidIdentitySurveyWeb,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeAndroidIdentitySurveyNtpAvatar,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeAndroidIdentitySurveyNtpPromo,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeAndroidIdentitySurveyBookmarkPromo,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
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
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kChromeIdentitySurveyProfileMenuSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeIdentitySurveyProfilePickerAddProfileSignin,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeIdentitySurveySigninInterceptProfileSeparation,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeIdentitySurveySigninPromoBubbleDismissed,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfileMenu,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfilePicker,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kChromeIdentitySurveyLaunchWithDelay,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kChromeIdentitySurveyLaunchWithDelayDuration,
                   &kChromeIdentitySurveyLaunchWithDelay,
                   "launch_delay_duration",
                   base::Milliseconds(3000));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kEnableASWebAuthenticationSession,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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
// Restricts the DBSC registration URL path to a single allowed string.
// Set to "/" to denote an empty path.
// Set to an empty string to remove the restriction.
const base::FeatureParam<std::string>
    kEnableBoundSessionCredentialsExclusiveRegistrationPath{
        &kEnableBoundSessionCredentials, "exclusive-registration-path", ""};
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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kEnableErrorBadgeOnIdentityDisc,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kEnableIdentityInAuthError, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables binding the OAuthMultilogin cookies to a device with DBSC prototype.
//
// If `kEnableOAuthMultiloginStandardCookiesBinding` is enabled, DBSC standard
// takes precedence over DBSC prototype.
BASE_FEATURE(kEnableOAuthMultiloginCookiesBinding,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// When enabled, Chrome will send a specific URL parameter to Gaia to trigger
// the server-side experiment for binding the OAuthMultilogin cookies to
// cryptographic keys.
//
// NOTE: This flag is meant to be used in conjunction with the
// `kEnableOAuthMultiloginCookiesBinding` flag.
BASE_FEATURE(kEnableOAuthMultiloginCookiesBindingServerExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kOAuthMultiloginCookieBindingEnforced,
                   &kEnableOAuthMultiloginCookiesBindingServerExperiment,
                   "enforced",
                   true);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables binding the OAuthMultilogin cookies to a device with DBSC standard.
//
// It takes precedence over the `kEnableOAuthMultiloginCookiesBinding` flag.
BASE_FEATURE(kEnableOAuthMultiloginStandardCookiesBinding,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Kill switch for enabling binding the OAuthMultilogin cookies to a device with
// DBSC standard for the Glic partition.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kEnableOAuthMultiloginStandardCookiesBindingForGlicPartition,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

BASE_FEATURE(kEnablePreferencesAccountStorage,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableSeamlessSignin, base::FEATURE_DISABLED_BY_DEFAULT);
// Determines the sign-in promo UI that is shown when kEnableSeamlessSignin is
// enabled.
constexpr base::FeatureParam<SeamlessSigninPromoType>::Option
    kSeamlessSigninPromoTypes[] = {
        {SeamlessSigninPromoType::kCompact, "compact"},
        {SeamlessSigninPromoType::kTwoButtons, "twoButtons"},
};
constexpr base::FeatureParam<SeamlessSigninPromoType> kSeamlessSigninPromoType{
    &kEnableSeamlessSignin, "seamless-signin-promo-type",
    SeamlessSigninPromoType::kCompact, &kSeamlessSigninPromoTypes};
// Determines the sign-in promo strings that are shown when
// kEnableSeamlessSignin is enabled.
constexpr base::FeatureParam<SeamlessSigninStringType>::Option
    kSeamlessSigninStringTypes[] = {
        {SeamlessSigninStringType::kContinueButton, "continueButton"},
        {SeamlessSigninStringType::kSigninButton, "signinButton"},
};
constexpr base::FeatureParam<SeamlessSigninStringType>
    kSeamlessSigninStringType{
        &kEnableSeamlessSignin, "seamless-signin-string-type",
        SeamlessSigninStringType::kContinueButton, &kSeamlessSigninStringTypes};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables the management disclaimer for managed signed profiles. All signed in
// profiles that never saw the management disclaimer will be shown the
// management disclaimer when they open Chrome. Every time the primary signed in
// account changes to a managed account, the management disclaimer will be
// shown. This is only for desktop platforms.
BASE_FEATURE(kEnforceManagementDisclaimer, base::FEATURE_DISABLED_BY_DEFAULT);
// The delay between policy registration retry.
const base::FeatureParam<base::TimeDelta>
    kPolicyDisclaimerRegistrationRetryDelay{
        &kEnforceManagementDisclaimer, "PolicyDisclaimerRegistrationRetryDelay",
        base::Hours(8)};
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kForcedDiceMigration, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kForceHistoryOptInScreen, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
BASE_FEATURE(kForceStartupSigninPromo, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kFRESignInAlternativeSecondaryButtonText,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kFullscreenSignInPromoUseDate, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kHandleMdmErrorsForDasherAccounts,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables a history sync educational tip in the magic stack on NTP.
BASE_FEATURE(kHistoryOptInEducationalTip, base::FEATURE_ENABLED_BY_DEFAULT);
// Determines which text should be shown on the history sync educational tip
// button. No-op unless HistoryOptInEducationalTip is enabled.
const base::FeatureParam<int> kHistoryOptInEducationalTipVariation(
    &kHistoryOptInEducationalTip,
    "history_opt_in_educational_tip_param",
    1);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMakeAccountsAvailableInIdentityManager,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// When enabled a new library is used to fetch accounts via
// AccountManagerAccountManagerDelegate
BASE_FEATURE(kMigrateAccountManagerDelegate, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kNonDefaultGaiaOriginCheck, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kOfferMigrationToDiceUsers, base::FEATURE_ENABLED_BY_DEFAULT);
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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kOpenAllProfilesFromProfilePickerExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kMaxProfilesCountToShowOpenAllButtonInProfilePicker{
        &kOpenAllProfilesFromProfilePickerExperiment,
        "max_profiles_count_to_show_open_all_button_in_profile_picker", 5};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kProfileCreationDeclineSigninCTAExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProfileCreationFrictionReductionExperimentPrefillNameRequirement,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProfileCreationFrictionReductionExperimentRemoveSigninStep,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProfileCreationFrictionReductionExperimentSkipCustomizeProfile,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProfilePickerTextVariations, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<ProfilePickerVariation>::Option
    kProfilePickerVariations[] = {
        {ProfilePickerVariation::kKeepWorkAndLifeSeparate,
         "keep-work-and-life-separate"},
        {ProfilePickerVariation::kGotAnotherGoogleAccount,
         "got-another-google-account"},
        {ProfilePickerVariation::kKeepTasksSeparate, "keep-tasks-separate"},
        {ProfilePickerVariation::kSharingAComputer, "sharing-a-computer"},
        {ProfilePickerVariation::kKeepEverythingInChrome,
         "keep-everything-in-chrome"},
};
constexpr base::FeatureParam<ProfilePickerVariation>
    kProfilePickerTextVariation{
        &kProfilePickerTextVariations, "profile-picker-variation",
        ProfilePickerVariation::kKeepWorkAndLifeSeparate,
        &kProfilePickerVariations};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BASE_FEATURE(kProfilesReordering, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kRollbackDiceMigration, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kShowProfilePickerToAllUsersExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BASE_FEATURE(kSigninPromoLimitsExperiment, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kContextualSigninPromoShownThreshold(
    &kSigninPromoLimitsExperiment,
    "contextual_signin_promo_shown_threshold",
    6);
const base::FeatureParam<int> kContextualSigninPromoDismissedThreshold(
    &kSigninPromoLimitsExperiment,
    "contextual_signin_promo_dismissed_threshold",
    2);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kSignInPromoMaterialNextUI, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BASE_FEATURE(kSigninWindows10DepreciationStateForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSigninWindows10DepreciationStateBypassForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsSigninWindows10DepreciationState() {
  // Bypass the feature for testing.
  if (base::FeatureList::IsEnabled(
          switches::kSigninWindows10DepreciationStateBypassForTesting)) {
    return false;
  }

  // Force enable the feature for testing.
  if (base::FeatureList::IsEnabled(
          switches::kSigninWindows10DepreciationStateForTesting)) {
    return true;
  }
#if BUILDFLAG(IS_WIN)
  return base::win::GetVersion() >= base::win::Version::WIN7 &&
         base::win::GetVersion() <= base::win::Version::WIN10_22H2;
#else
  return false;
#endif
}

#if BUILDFLAG(IS_ANDROID)
// Feature to bypass double-checking that signin callers have correctly gotten
// the user to accept account management. This check is slow and not strictly
// necessary, so disable it while we work on adding caching.
// TODO(https://crbug.com/339457762): Restore the check when we implement
// caching.
BASE_FEATURE(kSkipCheckForAccountManagementOnSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSkipRefreshTokenCheckInIdentityManager,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSmartEmailLineBreaking, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Enables the generation of pseudo-stable per-user per-device device
// identifiers. This identifier can be reset by the user by powerwashing the
// device.
BASE_FEATURE(kStableDeviceId, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kSupportAddSessionEmailPrefill, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Killswitch for the support of AddSession in web sign-in flow.
BASE_FEATURE(kSupportWebSigninAddSession, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncEnableBookmarksInTransportMode,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// When enabled, Chrome will always use the /IssueToken endpoint to fetch access
// tokens, no matter if a refresh token is bound or not.
BASE_FEATURE(kUseIssueTokenToFetchAccessTokens,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kWebSigninLeadsToImplicitlySignedInState,
             // THIS IS A TEST-ONLY FLAG AND SHOULD NEVER BE ENABLED.
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// keep-sorted end

bool IsExtensionsExplicitBrowserSigninEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

}  // namespace switches
