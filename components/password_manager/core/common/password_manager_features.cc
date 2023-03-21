// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace password_manager::features {
// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Enables biometric authentication before form filling.
BASE_FEATURE(kBiometricAuthenticationForFilling,
             "BiometricAuthenticationForFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
// Enables biometric authentication in settings.
BASE_FEATURE(kBiometricAuthenticationInSettings,
             "BiometricAuthenticationInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_FEATURE(kBiometricTouchToFill,
             "BiometricTouchToFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the overwriting of prefilled username fields if the server predicted
// the field to contain a placeholder value.
BASE_FEATURE(kEnableOverwritingPlaceholderUsernames,
             "EnableOverwritingPlaceholderUsernames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a second, Gaia-account-scoped password store for users who are signed
// in but not syncing.
BASE_FEATURE(kEnablePasswordsAccountStorage,
             "EnablePasswordsAccountStorage",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnablePasswordGenerationForClearTextFields,
             "EnablePasswordGenerationForClearTextFields",
             base::FEATURE_ENABLED_BY_DEFAULT);

// By default, Password Manager is enabled in fenced frames as part of
// FencedFramesAPIChanges blink experiment.
// This flag can be used via Finch to disable PasswordManager in the
// FencedFramesAPIChanges blink experiment without affecting the other
// features included in the experiment.
// TODO(crbug.com/1294378): Remove once launched.
BASE_FEATURE(kEnablePasswordManagerWithinFencedFrame,
             "EnablePasswordManagerWithinFencedFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables filling password on a website when there is saved password on
// affiliated website.
BASE_FEATURE(kFillingAcrossAffiliatedWebsites,
             "FillingAcrossAffiliatedWebsites",
#if !BUILDFLAG(IS_ANDROID) // Desktop and iOS
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
BASE_FEATURE(kFillOnAccountSelect,
             "fill-on-account-select",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_FEATURE(kPasswordManagerLogToTerminal,
             "PasswordManagerLogToTerminal",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// When enabled, initial sync will be forced during startup if the password
// store has encryption service failures.
BASE_FEATURE(kForceInitialSyncWhenDecryptionFails,
             "ForceInitialSyncWhenDecryptionFails",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables finding a confirmation password field during saving by inspecting the
// values of the fields. Used as a kill switch.
// TODO(crbug.com/1164861): Remove once confirmed to be safe (around M92 or so).
BASE_FEATURE(kInferConfirmationPasswordField,
             "InferConfirmationPasswordField",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// Removes the list of passwords from the Settings UI and adds a separate
// Password Manager view.
BASE_FEATURE(kIOSPasswordUISplit,
             "IOSPasswordUISplit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables password saving and filling in cross-origin iframes on IOS.
BASE_FEATURE(kIOSPasswordManagerCrossOriginIframeSupport,
             "IOSPasswordManagerCrossOriginIframeSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables displaying and managing compromised, weak and reused credentials in
// the Password Manager.
BASE_FEATURE(kIOSPasswordCheckup,
             "IOSPasswordCheckup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to show local/account storage in save/update password infobar
// subtitle.
BASE_FEATURE(kIOSShowPasswordStorageInSaveInfobar,
             "IOSShowPasswordStorageInSaveInfobar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables password bottom sheet to be displayed (on iOS) when a user is
// signed-in and taps on a username or password field on a website that has at
// least one credential saved in their password manager.
BASE_FEATURE(kIOSPasswordBottomSheet,
             "IOSPasswordBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // IS_IOS

// Enables memory mapping the word lists used in the zxcvbn library employed
// for the password weakness check.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kMemoryMapWeaknessCheckDictionaries,
             "MemoryMapWeaknessCheckDictionaries",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables new regex for OTP fields.
BASE_FEATURE(kNewRegexForOtpFields,
             "NewRegexForOtpFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the new password viewing subpage.
BASE_FEATURE(kPasswordViewPageInSettings,
             "PasswordViewPageInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending credentials from the settings UI.
BASE_FEATURE(kSendPasswords,
             "SendPasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables password leak detection for unauthenticated users.
BASE_FEATURE(kLeakDetectionUnauthenticated,
             "LeakDetectionUnauthenticated",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables .well-known based password change flow from leaked password dialog.
BASE_FEATURE(kPasswordChangeWellKnown,
             "PasswordChangeWellKnown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables import passwords flow from Chrome's settings page.
BASE_FEATURE(kPasswordImport,
             "PasswordImport",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kPasswordManagerRedesign,
             "PasswordManagerRedesign",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kPasswordsImportM2,
             "PasswordsImportM2",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables password reuse detection.
BASE_FEATURE(kPasswordReuseDetectionEnabled,
             "PasswordReuseDetectionEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables requesting and saving passwords grouping information from the
// affiliation service.
// TODO(crbug.com/1359392): Remove once launched.
BASE_FEATURE(kPasswordsGrouping,
             "PasswordsGrouping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing UI which allows users to easily revert their choice to
// never save passwords on a certain website.
BASE_FEATURE(kRecoverFromNeverSaveAndroid,
             "RecoverFromNeverSaveAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Enables a revamped version of the password management bubble triggered by
// manually clicking on the key icon in the omnibox.
BASE_FEATURE(kRevampedPasswordManagementBubble,
             "RevampedPasswordManagementBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables the password strength indicator.
BASE_FEATURE(kPasswordStrengthIndicator,
             "PasswordStrengthIndicator",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Displays at least the decryptable and never saved logins in the password
// manager
BASE_FEATURE(kSkipUndecryptablePasswords,
             "SkipUndecryptablePasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Use GMS AccountSettings to manage passkeys when UPM is not available.
BASE_FEATURE(kPasskeyManagementUsingAccountSettingsAndroid,
             "PasskeyManagementUsingAccountSettingsAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordEditDialogWithDetails,
             "PasswordEditDialogWithDetails",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Password generation bottom sheet.
BASE_FEATURE(kPasswordGenerationBottomSheet,
             "PasswordGenerationBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowUPMErrorNotification,
             "ShowUpmErrorNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the intent fetching for the credential manager in Google Mobile
// Services. It does not enable launching the credential manager.
BASE_FEATURE(kUnifiedCredentialManagerDryRun,
             "UnifiedCredentialManagerDryRun",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables use of Google Mobile Services for password storage. Chrome's local
// database will be unused but kept in sync for local passwords.
BASE_FEATURE(kUnifiedPasswordManagerAndroid,
             "UnifiedPasswordManagerAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing contextual error messages when UPM encounters an auth error.
BASE_FEATURE(kUnifiedPasswordManagerErrorMessages,
             "UnifiedPasswordManagerErrorMessages",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the built-in sync functionality in PasswordSyncBridge becomes
// unused, meaning that SyncService/SyncEngine will no longer download or
// upload changes to/from the Sync server. Instead, an external Android-specific
// backend will be used to achieve similar behavior.
BASE_FEATURE(kUnifiedPasswordManagerSyncUsingAndroidBackendOnly,
             "UnifiedPasswordManagerSyncUsingAndroidBackendOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automatic reenrollment into the Unified Password Manager for clients
// that were previously evicted after experiencing errors.
BASE_FEATURE(kUnifiedPasswordManagerReenrollment,
             "UnifiedPasswordManagerReenrollment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables all UI branding changes related to Unified Password Manager:
// the strings containing 'Password Manager' and the password manager
// icon.
BASE_FEATURE(kUnifiedPasswordManagerAndroidBranding,
             "UnifiedPasswordManagerAndroidBranding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables new exploratory strings for the save/update password prompts.
BASE_FEATURE(kExploratorySaveUpdatePasswordStrings,
             "ExploratorySaveUpdatePasswordStrings",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables support of sending additional votes on username first flow. The votes
// are sent on single password forms and contain information about preceding
// single username forms.
// TODO(crbug.com/959776): Clean up if the main crowdsourcing is good enough and
// we don't need additional signals.
BASE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing,
             "UsernameFirstFlowFallbackCrowdsourcing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables previewing password generation suggestion in the target form in
// cleartext.
BASE_FEATURE(kPasswordGenerationPreviewOnHover,
             "PasswordGenerationPreviewOnHover",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Current migration version to Google Mobile Services. If version saved in pref
// is lower than 'kMigrationVersion' passwords will be re-uploaded.
extern const base::FeatureParam<int> kMigrationVersion = {
    &kUnifiedPasswordManagerAndroid, "migration_version", 1};

// The maximum possible number of reenrollments into the UPM. Needed to avoid a
// patchy experience for users who experience errors in communication with
// Google Mobile Services on a regular basis.
extern const base::FeatureParam<int> kMaxUPMReenrollments = {
    &kUnifiedPasswordManagerReenrollment, "max_reenrollments", 0};

// The maximum possible number of reenrollment migration attempts. Needed to
// avoid wasting resources of users who have persistent errors.
extern const base::FeatureParam<int> kMaxUPMReenrollmentAttempts = {
    &kUnifiedPasswordManagerReenrollment, "max_reenrollment_attempts", 0};

// Whether to ignore the 24h timeout in between auth error messages as
// well as the 30 mins distance to sync error messages.
extern const base::FeatureParam<bool> kIgnoreAuthErrorMessageTimeouts = {
    &kUnifiedPasswordManagerErrorMessages, "ignore_auth_error_message_timeouts",
    false};

// The maximum number of authentication error UI messages to show before
// considering auth errors as unrecoverable and unenrolling the user from UPM.
// If this param is set, unenrollment will happen even if the auth error is in
// the ignore list.
// By default, there is no limit to how many errors will be shown.
extern const base::FeatureParam<int> kMaxShownUPMErrorsBeforeEviction = {
    &kUnifiedPasswordManagerErrorMessages,
    "max_shown_auth_errors_before_eviction", -1};

// The string version to use for the save/update password prompts when the user
// is syncing passwords. Version 1 is outdated, so the only supported versions
// currently are 2 and 3.
extern const base::FeatureParam<int> kSaveUpdatePromptSyncingStringVersion = {
    &kExploratorySaveUpdatePasswordStrings, "syncing_string_version", 2};
#endif

// Field trial identifier for password generation requirements.
const char kGenerationRequirementsFieldTrial[] =
    "PasswordGenerationRequirements";

// The file version number of password requirements files. If the prefix length
// changes, this version number needs to be updated.
// Default to 0 in order to get an empty requirements file.
const char kGenerationRequirementsVersion[] = "version";

// Length of a hash prefix of domain names. This is used to shard domains
// across multiple files.
// Default to 0 in order to put all domain names into the same shard.
const char kGenerationRequirementsPrefixLength[] = "prefix_length";

// Timeout (in milliseconds) for password requirements lookups. As this is a
// network request in the background that does not block the UI, the impact of
// high values is not strong.
// Default to 5000 ms.
const char kGenerationRequirementsTimeout[] = "timeout";

#if BUILDFLAG(IS_ANDROID)
bool UsesUnifiedPasswordManagerUi() {
  if (!base::FeatureList::IsEnabled(kUnifiedPasswordManagerAndroid))
    return false;
  UpmExperimentVariation variation = kUpmExperimentVariationParam.Get();
  switch (variation) {
    case UpmExperimentVariation::kEnableForSyncingUsers:
    case UpmExperimentVariation::kEnableForAllUsers:
      return true;
    case UpmExperimentVariation::kShadowSyncingUsers:
    case UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
      return false;
  }
  NOTREACHED() << "Define explicitly whether UI is required!";
  return false;
}

bool UsesUnifiedPasswordManagerBranding() {
  return (UsesUnifiedPasswordManagerUi() ||
          base::FeatureList::IsEnabled(kUnifiedPasswordManagerAndroidBranding));
}

bool RequiresMigrationForUnifiedPasswordManager() {
  if (!base::FeatureList::IsEnabled(kUnifiedPasswordManagerAndroid))
    return false;
  UpmExperimentVariation variation = kUpmExperimentVariationParam.Get();
  switch (variation) {
    case UpmExperimentVariation::kEnableForSyncingUsers:
    case UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
    case UpmExperimentVariation::kEnableForAllUsers:
      return true;
    case UpmExperimentVariation::kShadowSyncingUsers:
      return false;
  }
  NOTREACHED() << "Define explicitly whether migration is required!";
  return false;
}

bool ManagesLocalPasswordsInUnifiedPasswordManager() {
  if (!base::FeatureList::IsEnabled(kUnifiedPasswordManagerAndroid))
    return false;
  UpmExperimentVariation variation = kUpmExperimentVariationParam.Get();
  switch (variation) {
    case UpmExperimentVariation::kEnableForSyncingUsers:
    case UpmExperimentVariation::kShadowSyncingUsers:
    case UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
      return false;
    case UpmExperimentVariation::kEnableForAllUsers:
      return true;
  }
  NOTREACHED()
      << "Define explicitly whether local password management is supported!";
  return false;
}
#endif  // IS_ANDROID

#if BUILDFLAG(IS_IOS)
bool IsPasswordCheckupEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kIOSPasswordCheckup);
}
#endif  // IS_IOS

}  // namespace password_manager::features
