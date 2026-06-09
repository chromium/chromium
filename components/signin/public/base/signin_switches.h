// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"

class PrefService;

namespace switches {

// The switches should be documented alongside the definition of their values in
// the .cc file.

// Symbols must be annotated with COMPONENT_EXPORT(SIGNIN_SWITCHES) so that they
// can be exported by the signin_switches component. This prevents issues with
// component layering.

// Command line switches, sorted by name.

COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kClearTokenService[];

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kForceFreDefaultBrowserStep[];

COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kForceFreFeatureShowcaseSteps[];
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Feature declarations, sorted by the name of the BASE_DECLARE_FEATURE in each
// block. Please keep all FeatureParam declarations, enum class definitions, and
// helper function declarations for a given feature in the same
// newline-separated block as the feature declaration.
//
// clang-format off
// keep-sorted start allow_yaml_lists=yes case=no group_prefixes=["#if", "#else", "#endif", "extern const", "enum class", "};", "//", "bool", "base::", "BASE_DECLARE_FEATURE", "BASE_DECLARE_FEATURE_PARAM", "COMPONENT_EXPORT(SIGNIN_SWITCHES)"] by_regex=["BASE_DECLARE_FEATURE\\(.*\\);"] skip_lines=2
// clang-format on

#if BUILDFLAG(IS_IOS)
// When enabled, the account retrieval waits for accounts to become available on
// the first run after a restore operation.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kAccountRetrievalWaitsForRestoration);
#endif

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kAvatarButtonSyncPromoForTesting);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsAvatarSyncPromoFeatureEnabled();
COMPONENT_EXPORT(SIGNIN_SWITCHES)
base::TimeDelta GetAvatarSyncPromoFeatureMinimumCookeAgeParam();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// A HaTS survey flag for the survey to gather user feedback before any changes
// to the FRE as part of Chrome Desktop FRE Refresh project.
//
// NOTE: Only signed-in (excluding enterprise) users are eligible for this
// survey.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBeforeFirstRunDesktopRefreshSurvey);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBoundSessionCredentialsKillSwitch);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_IOS)
// Feature flag to build the External Privacy Context, which is used to provide
// the capability service with device signals.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBuildExternalPrivacyContext);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<std::string>
    kBuildExternalPrivacyContextAgeMismatchLearnMoreUrl;
#endif

#if BUILDFLAG(IS_IOS)
// Feature flag to enable caching identities in ios_internal.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kCacheIdentityListInChrome);
// Feature flag to prefetch and cache account capabilities.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableACPrefetch);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kCctSignInPrompt);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyFirstRun);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyWeb);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyNtpSigninButton);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeAndroidIdentitySurveyNtpSigninButtonProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyNtpAccountAvatarTap);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeAndroidIdentitySurveyNtpAccountAvatarTapProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyNtpPromo);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(double,
                           kChromeAndroidIdentitySurveyNtpPromoProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyBookmarkPromo);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeAndroidIdentitySurveyBookmarkPromoProbability);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables surveys to measure the effectiveness of the identity model.
// These surveys would be displayed after interactions such as signin, profile
// switching, etc. Please keep sorted alphabetically.
// LINT.IfChange
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyAddressBubbleSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(double,
                           kChromeIdentitySurveyAddressBubbleSigninProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyDiceWebSigninAccepted);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveyDiceWebSigninAcceptedProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyDiceWebSigninDeclined);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveyDiceWebSigninDeclinedProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyFirstRunSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(double,
                           kChromeIdentitySurveyFirstRunSigninProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyPasswordBubbleSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveyPasswordBubbleSigninProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyProfileMenuDismissed);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveyProfileMenuDismissedProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyProfileMenuSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(double,
                           kChromeIdentitySurveyProfileMenuSigninProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyProfilePickerAddProfileSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveyProfilePickerAddProfileSigninProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySigninInterceptProfileSeparation);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveySigninInterceptProfileSeparationProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySigninPromoBubbleDismissed);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveySigninPromoBubbleDismissedProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfileMenu);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveySwitchProfileFromProfileMenuProbability);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfilePicker);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    double,
    kChromeIdentitySurveySwitchProfileFromProfilePickerProbability);
// LINT.ThenChange(//chrome/browser/signin/signin_hats_util.cc)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Controls the duration for which the launch of an identity survey is delayed.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyLaunchWithDelay);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kChromeIdentitySurveyLaunchWithDelayDuration);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
// If enabled, the primary account consent level on ChromeOS is set to kSignin
// instead of kSync for new profiles.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeOsUseConsentLevelSigninForNewUsers);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Feature flag to enable cross-device sign-in.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kCrossDeviceSignin);
// Parameter containing the base URL for cross device sign-in.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<std::string> kCrossDeviceSigninUrl;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// If enabled, disables feedback for U18 users on desktop platforms.
// The iOS version is kDisableFeedbackForIneligibleUsers flag.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kDisableU18FeedbackDesktop);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Enables fetching and storing preview data for signed-in accounts.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableAccountPreviewData);

#if BUILDFLAG(IS_ANDROID)
// Whether activityless sign-in should be used for all entry points.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableActivitylessSigninAllEntryPoint);
#endif

#if BUILDFLAG(IS_ANDROID)
// After an account is added via the ADD_SESSION header it will be redirected to
// the specified URL.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableAddSessionRedirect);
#endif

#if BUILDFLAG(IS_IOS)
// Features to enable using the ASWebAuthenticationSession to add accounts to
// device.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableASWebAuthenticationSession);
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableBoundSessionCredentials);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<std::string>
    kEnableBoundSessionCredentialsExclusiveRegistrationPath;
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsBoundSessionCredentialsEnabled(const PrefService* profile_prefs);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableChromeRefreshTokenBinding);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsChromeRefreshTokenBindingEnabled(const PrefService* profile_prefs);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableChromeRefreshTokenBindingUpgrade);
enum class RefreshTokenBindingUpgradeType {
  kDarkLaunch,
  kLiveLaunch,
};
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kOamlCookieUpgradeEnabled);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(RefreshTokenBindingUpgradeType,
                           kRefreshTokenBindingUpgradeType);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !defined(NDEBUG) && !BUILDFLAG(IS_ANDROID)
// A fake feature corresponding to the kFakeCapabilityForTestingName account
// capability. This is only used in unit tests (and must be left disabled to
// prevent fetching the fake capability).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableFakeCapabilityForTesting);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableMtlsTokenBinding);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableOAuthMultiloginCookiesBinding);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableOAuthMultiloginCookiesBindingServerExperiment);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kOAuthMultiloginCookieBindingEnforced);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableOAuthMultiloginStandardCookiesBinding);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(
    kEnableOAuthMultiloginStandardCookiesBindingForSecondaryPartitions);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Enables a separate account-scoped storage for preferences.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnablePreferencesAccountStorage);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableSeamlessSignin);
enum class SeamlessSigninPromoType {
  // Compact design with a single button to sign in and a dropdown icon for
  // changing account.
  kCompact,
  // Design with a button to sign in and a button for changing account, similar
  // to the current promo.
  kTwoButtons,
};
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<SeamlessSigninPromoType>
    kSeamlessSigninPromoType;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableSearchAIModeSigninPromo);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<base::TimeDelta>
    kSearchAIModePromoPageLoadDelay;
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<base::TimeDelta> kSearchAIModePromoFrequency;
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSearchAIModeSignInPromoSelfDismissal);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableWebSigninLoadingDialog);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// Feature flag controlling whether the CanSignInToChrome account capability
// should be used to determine whether an account is eligible for sign-in.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnforceCanSignInToChromeCapability);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnforceManagementDisclaimer);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<base::TimeDelta>
    kPolicyDisclaimerRegistrationRetryDelay;
#endif


// Feature flag to fetch AccountInfo (UserInfo & Capabilities) on restart.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFetchAccountInfoOnRestart);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// This feature controls running visually refreshed first run and profile
// creation flows for users outside of the search engine choice regions. To
// enable the refresh in search engine choice screen regions,
// `kFirstRunDesktopChoiceScreenRefresh` needs to be enabled as well.
//
// Clients should never use this feature directly to determine if the
// refresh is enabled, they should use `IsFirstRunDesktopRefreshEnabled()`
// instead.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFirstRunDesktopRefresh);
// This feature controls running visually refreshed first run and profile
// creation flows, including the choice screen, for users in search engine
// choice screen regions. This feature is no-op if `kFirstRunDesktopRefresh` is
// disabled.
//
// Clients should never use this feature directly to determine if the
// refresh is enabled, they should use `IsFirstRunDesktopRefreshEnabled()`
// instead.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFirstRunDesktopChoiceScreenRefresh);
// Controls whether the First Run animations are disabled or not. If the feature
// is enabled, animations in the First Run are disabled, otherwise they're
// enabled. It should be only used for the testing purposes (e.g. pixel tests)
// and always disabled by default.
//
// NOTE: The tests must setup this feature in advance before the First Run flow
// starts, otherwise the animations will start.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kDisableFirstRunAnimationsForTesting);
// A helper function to determine if the first run desktop refresh is enabled
// (see `kFirstRunDesktopRefresh` and `kFirstRunDesktopChoiceScreenRefresh`
// flags).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsFirstRunDesktopRefreshEnabled(bool is_in_search_engine_choice_region);
// LINT.IfChange(FirstRunDesktopSignInPromoVariation)
enum class FirstRunDesktopSignInPromoVariation {
  // Default sign-in promo containing both sign-in and don't sign-in buttons
  // next to each other on the promo page.
  kDefault = 0,
  // Sign-in promo containing both sign-in and don't sign-in buttons but the
  // don't sign in button is moved to the top corner of the promo page and the
  // page informs the user they can create an account in the next step(s).
  kDontSignInInTheTopCorner = 1,
  // Sign-in promo containing only the sign-in button on the promo page. The
  // don't sign in button is moved to the Gaia page.
  kDontSignInOnGaiaPage = 2,
};
// LINT.ThenChange(//chrome/browser/resources/intro/sign_in_promo_refresh.ts:Variation)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<FirstRunDesktopSignInPromoVariation>
    kFirstRunDesktopSignInPromoVariation;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// A HaTS survey flag for the survey to gather user feedback after the changes
// introduced with `kFirstRunDesktopRefresh`.
//
// NOTE: Only signed-in (excluding enterprise) users are eligible for this
// survey.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFirstRunDesktopRefreshSurvey);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// It enables the first run revamp (introduce new UIs and additional effects).
// This feature is no-op if `kFirstRunDesktopRefresh` is disabled.
//
// Clients should never use this feature directly to determine if the
// revamp is enabled, they should use `IsFirstRunDesktopRevampEnabled`
// instead.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFirstRunDesktopRevamp);
// // A helper function to determine if the first run desktop revamp is enabled
// (see `kFirstRunDesktopRevamp`, `kFirstRunDesktopRefresh` and
// `kFirstRunDesktopChoiceScreenRefresh` flags).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsFirstRunDesktopRevampEnabled(bool is_in_search_engine_choice_region);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceHistoryOptInScreen);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceShowWebSigninLoadingDialog);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceStartupSigninPromo);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// TODO(crbug.com/408962000): This feature is going to be used after clients
// have the required information in local storage.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFullscreenSignInPromoUseDate);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kFullscreenSignInPromoUseDateInterval);
#endif

// When enabled, AccountInfo is required to have a CoreAccountId which is also
// guaranteed to be a Gaia Id.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kGaiaAccountIdEnforcement);

#if !BUILDFLAG(IS_IOS)
// When enabled, GLIC will check a new CanUseGeminiInChrome account capability
// to determine profile eligibility, instead of CanUseModelExecutionFeatures.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kGlicEligibilitySeparateAccountCapability);
#endif

// Feature to handle mdm errors on Enterprise and EDU accounts
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHandleMdmErrorsForDasherAccounts);

#if BUILDFLAG(IS_IOS)
// Killswitch for ignoring X-Chrome-Manage-Accounts header in subframes.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kIgnoreChromeManageAccountsInSubframes);

// Feature flag to ignore invalid grant errors in AuthenticationService.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kIgnoreInvalidGrantError);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Controls the MagiChrome sign-in banner.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMagiChromeSignInBanner);

// Controls experiments for MagiChrome (e.g. Gaia sign-in URL parameters).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMagiChromeSignInExperimentsBatch1);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
// Allow to switch the source of truth for accounts from AccountManagerFacade to
// IdentityManager.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMakeIdentityManagerSourceOfAccounts);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMigrateAccountManagerDelegate);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kNoAccountWebSignin);
#endif

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kNonDefaultGaiaOriginCheck);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Add new entry points for uploading passwords to account storage and update
// existing ones.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kPasswordUploadUiUpdate);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Experimenting with changing the secondary CTA for FRE and new profile
// creation.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfileCreationDeclineSigninCTAExperiment);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
// Shows the Signin Button Profile Disc in the toolbar on all pages, rather than
// solely the NTP. See crbug.com/495820974.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfileDiscOnAllPages);
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfilesReordering);

#if BUILDFLAG(IS_IOS)
// Feature flag controlling whether Chrome uses the contextual version of
// relevant account capabilities on supported platforms.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kReadContextualAccountCapabilities);
#endif

// Enables fetching the capability of the same name on all platforms.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kReadSupportsWalletPrivatePassesInAutofillCapability);

#if !BUILDFLAG(IS_ANDROID)
// Kill switch for Device Management Service OAuth scope.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kRestrictDeviceManagementServiceOAuthScope);
#endif  // !BUILDFLAG(IS_ANDROID)

// Enables the new visual design for the profile switch interception bubble,
// aligning it with the V2 style used for new profiles. Used in
// dice_web_signin_intercept_handler.cc.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninInterceptGraphicUpdate);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
// Experiment replacing signed out avatar with signin button on Android, see
// crbug.com/475816843.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninLevelUpButton);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninManagerSeedingFix);
#endif  // BUILDFLAG(IS_ANDROID)

// Feature to control the experiment for max count of showing contextual sign-in
// promos and UNO bubble reprompt.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninPromoLimitsExperiment);
// Param that controls the threshold of the contextual sign in promo shown
// limit for the experiment.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<int> kContextualSigninPromoShownThreshold;
// Param that controls the threshold of the contextual sign in promos dismissed
// limit for the experiment.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<int> kContextualSigninPromoDismissedThreshold;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Uses the Material Next theme for the signin promo.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSignInPromoMaterialNextUI);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Feature to show a promo on the avatar pill on profile startup.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninPromoOnAvatarPill);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSigninPromoOnAvatarPillStartupDelayForPromoShow);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSigninPromoOnAvatarPillDelayForNextPromoAllowed);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Feature flag used for testing purposes only:
//
// Set this flag to force the flow on any platform.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninWindows10DepreciationStateForTesting);
// Set this flag to force the flow off on Windows 10 (a lot of bots run on
// Windows 10) - to avoid having generic tests having a per platform
// expectations.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninWindows10DepreciationStateBypassForTesting);
COMPONENT_EXPORT(SIGNIN_SWITCHES) bool IsSigninWindows10DepreciationState();

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSkipCheckForAccountManagementOnSignin);
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSkipRefreshTokenCheckInIdentityManager);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSmartEmailLineBreaking);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kStableDeviceId);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_IOS)
// Killswitch for the feature to prefill the email of the account to add when
// opening the "add account" flow for an ADDSESSION header.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSupportAddSessionEmailPrefill);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Kill switch for displaying sign-in errors in the profile picker.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSupportErrorsInProfilePicker);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSupportForcedSigninPolicy);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSupportWebSigninAddSession);
#endif  // BUILDFLAG(IS_ANDROID)

// This gates the new single-model approach where account bookmarks are stored
// in separate permanent folders in BookmarkModel. The flag controls whether
// BOOKMARKS datatype is enabled in the transport mode.
// TODO(crbug.com/40943550): Remove this.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSyncEnableBookmarksInTransportMode);
// This feature flag is used as a subset of the original code that was behind
// `kSyncEnableBookmarksInTransportMode` that introduced changes that are not
// directly related to Transport Mode. Mostly the changes are Ui-visible and
// will be migrated to be using this flag instead. This will allow to run
// a Finch study on Cros and launch independently of TransportMode on Cros. The
// flag is enabled by default on Windows/Mac/Linux.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBookmarksMigrateUiChanges);

// If enabled, buttons for sign-in promos / intercepts will use consistent
// primary - tonal button class pattern.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUsePrimaryAndTonalButtonsForPromos);

#if BUILDFLAG(IS_ANDROID)
enum class SeamlessSigninStringType {
  // Strings with "Sign in to Chrome" in the title and "Continue as" in the
  // primary button
  kContinueButton,
  // Strings with "Sign in to Chrome" in the description and "Sign in as" in the
  // primary button
  kSigninButton,
};
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<SeamlessSigninStringType>
    kSeamlessSigninStringType;
#endif  // BUILDFLAG(IS_ANDROID)

// keep-sorted end

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
