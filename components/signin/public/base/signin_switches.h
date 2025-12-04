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
// Feature to allowlist certain scopes for which mdm errors will be shown. All
// other scopes will be ignored.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kAllowlistScopesForMdmErrors);
#endif

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kAvatarButtonSyncPromo);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kAvatarButtonSyncPromoMinimumCookieAgeParam);
#endif
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kAvatarButtonSyncPromoForTesting);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsAvatarSyncPromoFeatureEnabled();
COMPONENT_EXPORT(SIGNIN_SWITCHES)
base::TimeDelta GetAvatarSyncPromoFeatureMinimumCookeAgeParam();

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBoundSessionCredentialsKillSwitch);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

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
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyNtpAvatar);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyNtpPromo);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeAndroidIdentitySurveyBookmarkPromo);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables surveys to measure the effectiveness of the identity model.
// These surveys would be displayed after interactions such as signin, profile
// switching, etc. Please keep sorted alphabetically.
// LINT.IfChange
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyAddressBubbleSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyDiceWebSigninAccepted);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyDiceWebSigninDeclined);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyFirstRunSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyPasswordBubbleSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyProfileMenuDismissed);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyProfileMenuSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveyProfilePickerAddProfileSignin);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySigninInterceptProfileSeparation);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySigninPromoBubbleDismissed);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfileMenu);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kChromeIdentitySurveySwitchProfileFromProfilePicker);
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

#if BUILDFLAG(IS_IOS)
// Show the error badge on the identity disc in the NTP.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableErrorBadgeOnIdentityDisc);
#endif

#if BUILDFLAG(IS_IOS)
// Features to enable identities in auth error (stale token).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableIdentityInAuthError);
#endif

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
    kEnableOAuthMultiloginStandardCookiesBindingForGlicPartition);
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
BASE_DECLARE_FEATURE(kEnforceManagementDisclaimer);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<base::TimeDelta>
    kPolicyDisclaimerRegistrationRetryDelay;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// When enabled, forces the users out of the implicitly signed-in state - either
// signing them out of Chromium (i.e. signed into web-only) or explicitly
// signing them in.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForcedDiceMigration);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceHistoryOptInScreen);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceStartupSigninPromo);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFRESignInAlternativeSecondaryButtonText);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// TODO(crbug.com/408962000): This feature is going to be used after clients
// have the required information in local storage.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kFullscreenSignInPromoUseDate);
#endif

// Feature to handle mdm errors on Enterprise and EDU accounts
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHandleMdmErrorsForDasherAccounts);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHistoryOptInEducationalTip);
extern const base::FeatureParam<int> kHistoryOptInEducationalTipVariation;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMakeAccountsAvailableInIdentityManager);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMigrateAccountManagerDelegate);
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kNonDefaultGaiaOriginCheck);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// When enabled, an implicitly signed-in user will be offered a dialog to
// migrate to explicit browser sign-in.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kOfferMigrationToDiceUsers);
// The minimum delay after a browser startup before the dialog can be shown.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kOfferMigrationToDiceUsersMinDelay);
// The maximum delay after a browser startup before the dialog can be shown.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kOfferMigrationToDiceUsersMaxDelay);
// The minimum time from the last time the dialog was shown before it can be
// shown again.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kOfferMigrationToDiceUsersMinTimeBetweenDialogs);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Experimenting with a button to all profiles from the profile picker.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kOpenAllProfilesFromProfilePickerExperiment);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<int>
    kMaxProfilesCountToShowOpenAllButtonInProfilePicker;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Experimenting with changing the secondary CTA for FRE and new profile
// creation.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfileCreationDeclineSigninCTAExperiment);

// Experimenting with prefill name requirement for the profile creation flow.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(
    kProfileCreationFrictionReductionExperimentPrefillNameRequirement);

// Experimenting with removing signin in the profile creation flow.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(
    kProfileCreationFrictionReductionExperimentRemoveSigninStep);

// Experimenting with removing the profile customization bubble in the profile
// creation flow.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(
    kProfileCreationFrictionReductionExperimentSkipCustomizeProfile);

// Enables variations of the profile picker text.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfilePickerTextVariations);
enum class ProfilePickerVariation {
  kKeepWorkAndLifeSeparate = 0,
  kGotAnotherGoogleAccount = 1,
  kKeepTasksSeparate = 2,
  kSharingAComputer = 3,
  kKeepEverythingInChrome = 4,
};
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<ProfilePickerVariation>
    kProfilePickerTextVariation;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfilesReordering);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// When enabled, rolls back the DICe migration for implicitly signed-in users.
// Overrides `kOfferMigrationToDiceUsers` and `kForcedDiceMigration`.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kRollbackDiceMigration);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Experimenting with showing the profile picker to all users (not only the
// users with multiple profiles).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kShowProfilePickerToAllUsersExperiment);

// Feature to control the experiment for max count of showing contextual sign-in
// promos and UNO bubble reprompt.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSigninPromoLimitsExperiment);
// Param that controls the threshold of the contextual sign in promos shown
// limit for the experiment.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<int> kContextualSigninPromoShownThreshold;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Uses the Material Next theme for the signin promo.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSignInPromoMaterialNextUI);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSupportWebSigninAddSession);
#endif  // BUILDFLAG(IS_ANDROID)

// This gates the new single-model approach where account bookmarks are stored
// in separate permanent folders in BookmarkModel. The flag controls whether
// BOOKMARKS datatype is enabled in the transport mode.
// TODO(crbug.com/40943550): Remove this.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSyncEnableBookmarksInTransportMode);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUseIssueTokenToFetchAccessTokens);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// If enabled, web sign-in will implicitly sign the user in.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kWebSigninLeadsToImplicitlySignedInState);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

// Helper functions that are no longer attached to any features.

// Returns if the current browser supports an explicit sign in (signs the user
// into transport mode, as defined above) for extension access points (e.g. the
// `ExtensionPostInstallDialogDelegate`).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsExtensionsExplicitBrowserSigninEnabled();

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
