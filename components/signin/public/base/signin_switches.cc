// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

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

BASE_FEATURE(kHideSettingsSignInPromo,
             "HideSettingsSignInPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseConsentLevelSigninForLegacyAccountEmailPref,
             "UseConsentLevelSigninForLegacyAccountEmailPref",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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
        &kEnableBoundSessionCredentials, "exclusive-registration-path",
        "/RegisterSession"};

// Enables Chrome refresh tokens binding to a device. Requires
// "EnableBoundSessionCredentials" being enabled as a prerequisite.
BASE_FEATURE(kEnableChromeRefreshTokenBinding,
             "EnableChromeRefreshTokenBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeRefreshTokenBindingEnabled(const PrefService* profile_prefs) {
  return IsBoundSessionCredentialsEnabled(profile_prefs) &&
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
// Rewrites DefaultSearchEnginePromoDialog into MVC pattern.
BASE_FEATURE(kSearchEnginePromoDialogRewrite,
             "SearchEnginePromoDialogRewrite",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kExplicitBrowserSigninUIOnDesktop,
             "ExplicitBrowserSigninUIOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kInterceptBubblesDismissibleByAvatarButton{
    &kExplicitBrowserSigninUIOnDesktop,
    /*name=*/"bubble_dismissible_by_avatar_button",
    /*default_value=*/true};

bool IsExplicitBrowserSigninUIOnDesktopEnabled() {
  return base::FeatureList::IsEnabled(kExplicitBrowserSigninUIOnDesktop);
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
BASE_FEATURE(kUseSystemCapabilitiesForMinorModeRestrictions,
             "UseSystemCapabilitiesForMinorModeRestrictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr int kFetchImmediatelyAvailableCapabilityDeadlineDefaultValueMs = 100;

const base::FeatureParam<int> kFetchImmediatelyAvailableCapabilityDeadlineMs{
    &kUseSystemCapabilitiesForMinorModeRestrictions,
    /*name=*/"FetchImmediatelyAvailableCapabilityDeadlineMs",
    kFetchImmediatelyAvailableCapabilityDeadlineDefaultValueMs};

BASE_FEATURE(kRemoveSignedInAccountsDialog,
             "RemoveSignedInAccountsDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kPreconnectAccountCapabilitiesPostSignin,
             "PreconnectAccountCapabilitiesPostSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Flag guarding the refresh of the metrics services states after the related
// prefs are restored during the device restoration, to enable metrics upload
// if it's allowed by those restored prefs.
BASE_FEATURE(kUpdateMetricsServicesStateInRestore,
             "UpdateMetricsServicesStateInRestore",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace switches

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Enables the new style, "For You" First Run Experience
BASE_FEATURE(kForYouFre, "ForYouFre", base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
constexpr base::FeatureParam<WithDefaultBrowserStep>::Option
    kWithDefaultBrowserStepOptions[] = {
        {WithDefaultBrowserStep::kYes, "yes"},
        {WithDefaultBrowserStep::kNo, "no"},
        {WithDefaultBrowserStep::kForced, "forced"},
};

const base::FeatureParam<WithDefaultBrowserStep>
    kForYouFreWithDefaultBrowserStep{
        &kForYouFre, /*name=*/"with_default_browser_step",
        /*default_value=*/WithDefaultBrowserStep::kYes,
        /*options=*/&kWithDefaultBrowserStepOptions};

constexpr base::FeatureParam<DefaultBrowserVariant>::Option
    kDefaultBrowserVariantOptions[] = {
        {DefaultBrowserVariant::kCurrent, "current"},
        {DefaultBrowserVariant::kNew, "new"},
};

const base::FeatureParam<DefaultBrowserVariant> kForYouFreDefaultBrowserVariant{
    &kForYouFre, /*name=*/"default_browser_variant",
    /*default_value=*/DefaultBrowserVariant::kNew,
    /*options=*/&kDefaultBrowserVariantOptions};

// Feature that indicates that we should put the client in a study group
// (provided through `kForYouFreStudyGroup`) to be able to look at metrics in
// the long term. Does not affect the client's behavior by itself, instead this
// is done through the `kForYouFre` feature.
BASE_FEATURE(kForYouFreSyntheticTrialRegistration,
             "ForYouFreSyntheticTrialRegistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// String that refers to the study group in which this install was enrolled.
// Used to implement the sticky experiment tracking. If the value is an empty
// string, we don't register the client.
const base::FeatureParam<std::string> kForYouFreStudyGroup{
    &kForYouFreSyntheticTrialRegistration, /*name=*/"group_name",
    /*default_value=*/""};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables the generation of pseudo-stable per-user per-device device
// identifiers. This identifier can be reset by the user by powerwashing the
// device.
BASE_FEATURE(kStableDeviceId,
             "StableDeviceId",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Enables showing the enterprise dialog after every signin into a managed
// account.
BASE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin,
             "ShowEnterpriseDialogForAllManagedAccountsSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables signout for enteprise managed profiles
BASE_FEATURE(kDisallowManagedProfileSignout,
             "DisallowManagedProfileSignout",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kVerifyRequestInitiatorForMirrorHeaders,
             "VerifyRequestInitiatorForMirrorHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kProfilesReordering,
             "ProfilesReordering",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kForceSigninFlowInProfilePicker,
             "ForceSigninFlowInProfilePicker",
             base::FEATURE_ENABLED_BY_DEFAULT);
extern const base::FeatureParam<bool>
    kForceSigninReauthInProfilePickerUseAddSession{
        &kForceSigninFlowInProfilePicker, /*name=*/"reauth_use_add_session",
        /*default_value=*/false};
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
