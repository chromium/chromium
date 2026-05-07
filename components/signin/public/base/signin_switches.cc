// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
// Feature param to pass probability for identity surveys.
constexpr char kHatsSurveyProbabilityName[] = "probability";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID)

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
          base::win::GetVersion() <= base::win::Version::WIN10_22H2);
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
  return base::Days(14);
#else
  NOTREACHED();
#endif
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kBeforeFirstRunDesktopRefreshSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Allows to disable the bound session credentials code in case of emergency.
BASE_FEATURE(kBoundSessionCredentialsKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kBuildExternalPrivacyContext, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kBuildExternalPrivacyContextAgeMismatchLearnMoreUrl{
        &kBuildExternalPrivacyContext, "AgeMismatchLearnMoreUrl",
        "https://support.google.com/families/answer/"
        "7087030#zippy=%2Ciphone-and-ipad"};
#endif

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kCacheIdentityListInChrome, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableACPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCctSignInPrompt, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// The probabilities are calculated based on a 1% stable experiment and scaling
// the response count for 1 week to give around 2000 responses per milestone for
// total stable population.
BASE_FEATURE(kChromeAndroidIdentitySurveyFirstRun,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeAndroidIdentitySurveyWeb,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeAndroidIdentitySurveyNtpSigninButton,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeAndroidIdentitySurveyNtpSigninButtonProbability,
                   &kChromeAndroidIdentitySurveyNtpSigninButton,
                   kHatsSurveyProbabilityName,
                   0.026);
BASE_FEATURE(kChromeAndroidIdentitySurveyNtpAccountAvatarTap,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeAndroidIdentitySurveyNtpAccountAvatarTapProbability,
                   &kChromeAndroidIdentitySurveyNtpAccountAvatarTap,
                   kHatsSurveyProbabilityName,
                   0.003);
BASE_FEATURE(kChromeAndroidIdentitySurveyNtpPromo,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeAndroidIdentitySurveyNtpPromoProbability,
                   &kChromeAndroidIdentitySurveyNtpPromo,
                   kHatsSurveyProbabilityName,
                   0.048);
BASE_FEATURE(kChromeAndroidIdentitySurveyBookmarkPromo,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeAndroidIdentitySurveyBookmarkPromoProbability,
                   &kChromeAndroidIdentitySurveyBookmarkPromo,
                   kHatsSurveyProbabilityName,
                   0.42);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr double kMediumSurveyProbability = 0.08;
constexpr double kLowSurveyProbability = 0.008;
BASE_FEATURE(kChromeIdentitySurveyAddressBubbleSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyAddressBubbleSigninProbability,
                   &kChromeIdentitySurveyAddressBubbleSignin,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyDiceWebSigninAccepted,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyDiceWebSigninAcceptedProbability,
                   &kChromeIdentitySurveyDiceWebSigninAccepted,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyDiceWebSigninDeclined,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyDiceWebSigninDeclinedProbability,
                   &kChromeIdentitySurveyDiceWebSigninDeclined,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyFirstRunSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyFirstRunSigninProbability,
                   &kChromeIdentitySurveyFirstRunSignin,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyPasswordBubbleSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyPasswordBubbleSigninProbability,
                   &kChromeIdentitySurveyPasswordBubbleSignin,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyProfileMenuDismissed,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyProfileMenuDismissedProbability,
                   &kChromeIdentitySurveyProfileMenuDismissed,
                   kHatsSurveyProbabilityName,
                   kLowSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyProfileMenuSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveyProfileMenuSigninProbability,
                   &kChromeIdentitySurveyProfileMenuSignin,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveyProfilePickerAddProfileSignin,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveyProfilePickerAddProfileSigninProbability,
    &kChromeIdentitySurveyProfilePickerAddProfileSignin,
    kHatsSurveyProbabilityName,
    kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveySigninInterceptProfileSeparation,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveySigninInterceptProfileSeparationProbability,
    &kChromeIdentitySurveySigninInterceptProfileSeparation,
    kHatsSurveyProbabilityName,
    kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveySigninPromoBubbleDismissed,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveySigninPromoBubbleDismissedProbability,
                   &kChromeIdentitySurveySigninPromoBubbleDismissed,
                   kHatsSurveyProbabilityName,
                   kMediumSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfileMenu,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kChromeIdentitySurveySwitchProfileFromProfileMenuProbability,
                   &kChromeIdentitySurveySwitchProfileFromProfileMenu,
                   kHatsSurveyProbabilityName,
                   kLowSurveyProbability);
BASE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfilePicker,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveySwitchProfileFromProfilePickerProbability,
    &kChromeIdentitySurveySwitchProfileFromProfilePicker,
    kHatsSurveyProbabilityName,
    kMediumSurveyProbability);
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

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kChromeOsUseConsentLevelSigninForNewUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kCrossDeviceSignin, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kCrossDeviceSigninUrl{&kCrossDeviceSignin,
                                                            "url", ""};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kDisableU18FeedbackDesktop, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
// Whether activityless sign-in should be used for all entry points.
BASE_FEATURE(kEnableActivitylessSigninAllEntryPoint,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableAddSessionRedirect, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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

#if !defined(NDEBUG)
BASE_FEATURE(kEnableFakeCapabilityForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables mTLS token binding in the identity stack. This allows binding tokens
// to an mTLS certificate upon receiving the `mtl_token_binding` indicator in
// the Dice sigin header.
BASE_FEATURE(kEnableMtlsTokenBinding, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables binding the OAuthMultilogin cookies to a device with DBSC prototype.
//
// If `kEnableOAuthMultiloginStandardCookiesBinding` is enabled, DBSC standard
// takes precedence over DBSC prototype.
BASE_FEATURE(kEnableOAuthMultiloginCookiesBinding,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_WIN)
);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// When enabled, Chrome will send a specific URL parameter to Gaia to trigger
// the server-side experiment for binding the OAuthMultilogin cookies to
// cryptographic keys.
//
// NOTE: This flag is meant to be used in conjunction with the
// `kEnableOAuthMultiloginCookiesBinding` flag.
BASE_FEATURE(kEnableOAuthMultiloginCookiesBindingServerExperiment,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_WIN)
);
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

// Enables binding the OAuthMultilogin cookies to a device with DBSC standard
// for secondary partitions.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kEnableOAuthMultiloginStandardCookiesBindingForSecondaryPartitions,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

BASE_FEATURE(kEnablePreferencesAccountStorage,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableSeamlessSignin, base::FEATURE_ENABLED_BY_DEFAULT);
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
BASE_FEATURE(kEnableSearchAIModeSigninPromo, base::FEATURE_DISABLED_BY_DEFAULT);
// The delay we allow for the AIM search result to load before we display the
// sign-in promo bubble.
const base::FeatureParam<base::TimeDelta> kSearchAIModePromoPageLoadDelay{
    &kEnableSearchAIModeSigninPromo, "SearchAIModePromoPageLoadDelay",
    base::Seconds(4)};
// The gap between impressions of the Signin AI Search Mode promo.
const base::FeatureParam<base::TimeDelta> kSearchAIModePromoFrequency{
    &kEnableSearchAIModeSigninPromo, "SearchAIModePromoFrequency",
    base::Days(14)};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableWebSigninLoadingDialog, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kEnforceCanSignInToChromeCapability,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Enables the management disclaimer for managed signed profiles. All signed in
// profiles that never saw the management disclaimer will be shown the
// management disclaimer when they open Chrome. Every time the primary signed in
// account changes to a managed account, the management disclaimer will be
// shown. This is only for desktop platforms.
BASE_FEATURE(kEnforceManagementDisclaimer, base::FEATURE_ENABLED_BY_DEFAULT);
// The delay between policy registration retry.
const base::FeatureParam<base::TimeDelta>
    kPolicyDisclaimerRegistrationRetryDelay{
        &kEnforceManagementDisclaimer, "PolicyDisclaimerRegistrationRetryDelay",
        base::Hours(8)};
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kFirstRunDesktopRefresh, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFirstRunDesktopChoiceScreenRefresh,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDisableFirstRunAnimationsForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsFirstRunDesktopRefreshEnabled(bool is_in_search_engine_choice_region) {
  if (is_in_search_engine_choice_region &&
      !base::FeatureList::IsEnabled(kFirstRunDesktopChoiceScreenRefresh)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kFirstRunDesktopRefresh);
}
constexpr base::FeatureParam<FirstRunDesktopSignInPromoVariation>::Option
    kFirstRunDesktopSignInPromoVariations[] = {
        {FirstRunDesktopSignInPromoVariation::kDefault, "default"},
        {FirstRunDesktopSignInPromoVariation::kDontSignInInTheTopCorner,
         "dont-sign-in-in-the-top-corner"},
        {FirstRunDesktopSignInPromoVariation::kDontSignInOnGaiaPage,
         "dont-sign-in-on-gaia-page"},
};
constexpr base::FeatureParam<FirstRunDesktopSignInPromoVariation>
    kFirstRunDesktopSignInPromoVariation{
        &kFirstRunDesktopRefresh, "sign-in-promo-variation",
        FirstRunDesktopSignInPromoVariation::kDefault,
        &kFirstRunDesktopSignInPromoVariations};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kFirstRunDesktopRefreshSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kFirstRunDesktopRevamp, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsFirstRunDesktopRevampEnabled(bool is_in_search_engine_choice_region) {
  return IsFirstRunDesktopRefreshEnabled(is_in_search_engine_choice_region) &&
         base::FeatureList::IsEnabled(kFirstRunDesktopRevamp);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kForceHistoryOptInScreen, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceShowWebSigninLoadingDialog,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
BASE_FEATURE(kForceStartupSigninPromo, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kFullscreenSignInPromoUseDate, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFullscreenSignInPromoUseDateInterval,
                   &kFullscreenSignInPromoUseDate,
                   "interval",
                   -1);
#endif

BASE_FEATURE(kGaiaAccountIdEnforcement, base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_IOS)
BASE_FEATURE(kGlicEligibilitySeparateAccountCapability,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kHandleMdmErrorsForDasherAccounts,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kIgnoreChromeManageAccountsInSubframes,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to ignore invalid grant errors in AuthenticationService.
BASE_FEATURE(kIgnoreInvalidGrantError, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kMagiChromeSignInExperimentsBatch1,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMakeIdentityManagerSourceOfAccounts,
             base::FEATURE_DISABLED_BY_DEFAULT);
// When enabled a new library is used to fetch accounts via
// AccountManagerAccountManagerDelegate
BASE_FEATURE(kMigrateAccountManagerDelegate, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kNoAccountWebSignin, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

BASE_FEATURE(kNonDefaultGaiaOriginCheck, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kPasswordUploadUiUpdate, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProfileCreationDeclineSigninCTAExperiment,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kProfileDiscOnAllPages, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
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

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kReadContextualAccountCapabilities,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRestrictDeviceManagementServiceOAuthScope,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kShowProfilePickerToAllUsersExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSigninLevelUpButton, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSigninManagerSeedingFix, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kSigninPromoOnAvatarPill, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSigninPromoOnAvatarPillStartupDelayForPromoShow,
                   &kSigninPromoOnAvatarPill,
                   "startup_delay_for_promo_show",
                   base::Seconds(30));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSigninPromoOnAvatarPillDelayForNextPromoAllowed,
                   &kSigninPromoOnAvatarPill,
                   "delay_for_next_promo_allowed",
                   base::Days(7));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Kill switch for displaying sign-in errors in the profile picker.
BASE_FEATURE(kSupportErrorsInProfilePicker, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSupportForcedSigninPolicy, base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kBookmarksMigrateUiChanges, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUsePrimaryAndTonalButtonsForPromos,
             base::FEATURE_ENABLED_BY_DEFAULT);

// keep-sorted end

}  // namespace switches
