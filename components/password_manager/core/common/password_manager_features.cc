// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "build/build_config.h"

namespace password_manager::features {
// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
const base::Feature kBiometricTouchToFill = {"BiometricTouchToFill",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables submission detection for forms dynamically cleared but not removed
// from the page.
const base::Feature kDetectFormSubmissionOnFormClear = {
    "DetectFormSubmissionOnFormClear",
#if BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Force enables password change capabilities for every domain, regardless of
// the server response. The flag is meant for end-to-end testing purposes only.
const base::Feature kForceEnablePasswordDomainCapabilities = {
    "ForceEnablePasswordDomainCapabilities", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables favicons in Password Manager.
const base::Feature kEnableFaviconForPasswords{
    "EnableFaviconForPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the overwriting of prefilled username fields if the server predicted
// the field to contain a placeholder value.
const base::Feature kEnableOverwritingPlaceholderUsernames{
    "EnableOverwritingPlaceholderUsernames", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a second, Gaia-account-scoped password store for users who are signed
// in but not syncing.
const base::Feature kEnablePasswordsAccountStorage = {
    "EnablePasswordsAccountStorage",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature KEnablePasswordGenerationForClearTextFields = {
    "EnablePasswordGenerationForClearTextFields",
    base::FEATURE_ENABLED_BY_DEFAULT};

// By default, Password Manager is disabled in fenced frames for now.
// TODO(crbug.com/1294378): Remove once launched.
const base::Feature kEnablePasswordManagerWithinFencedFrame{
    "EnablePasswordManagerWithinFencedFrame",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables filling password on a website when there is saved password on
// affiliated website.
const base::Feature kFillingAcrossAffiliatedWebsites{
    "FillingAcrossAffiliatedWebsites", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
const base::Feature kFillOnAccountSelect = {"fill-on-account-select",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_LINUX)
// When enabled, initial sync will be forced during startup if the password
// store has encryption service failures.
const base::Feature kForceInitialSyncWhenDecryptionFails = {
    "ForceInitialSyncWhenDecryptionFails", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables finding a confirmation password field during saving by inspecting the
// values of the fields. Used as a kill switch.
// TODO(crbug.com/1164861): Remove once confirmed to be safe (around M92 or so).
const base::Feature kInferConfirmationPasswordField = {
    "InferConfirmationPasswordField", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature flag that updates icons, strings, and views for Google Password
// Manager.
const base::Feature kIOSEnablePasswordManagerBrandingUpdate{
    "IOSEnablePasswordManagerBrandingUpdate",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables (un)muting compromised passwords from bulk leak check in settings.
const base::Feature kMuteCompromisedPasswords{
    "MuteCompromisedPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables adding, displaying and modifying extra notes to stored credentials.
const base::Feature kPasswordNotes{"PasswordNotes",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables sending credentials from the settings UI.
const base::Feature kSendPasswords{"SendPasswords",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password leak detection for unauthenticated users.
const base::Feature kLeakDetectionUnauthenticated = {
    "LeakDetectionUnauthenticated", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables automatic password change flow from leaked password dialog.
const base::Feature kPasswordChange = {"PasswordChange",
#if BUILDFLAG(IS_ANDROID)
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#else
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables password change flow from bulk leak check in settings.
const base::Feature kPasswordChangeInSettings = {
    "PasswordChangeInSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables .well-known based password change flow from leaked password dialog.
const base::Feature kPasswordChangeWellKnown = {
    "PasswordChangeWellKnown", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching credentials capabilities from server for the
// |PasswordChangeInSettings| and |PasswordChange| features.
const base::Feature kPasswordDomainCapabilitiesFetching = {
    "PasswordDomainCapabilitiesFetching", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the ability to import passwords from Chrome's settings page.
const base::Feature kPasswordImport = {"PasswordImport",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password reuse detection.
const base::Feature kPasswordReuseDetectionEnabled = {
    "PasswordReuseDetectionEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a revised opt-in flow for the account-scoped password storage.
const base::Feature kPasswordsAccountStorageRevisedOptInFlow = {
    "PasswordsAccountStorageRevisedOptInFlow",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password scripts fetching for the |PasswordChangeInSettings| feature.
const base::Feature kPasswordScriptsFetching = {
    "PasswordScriptsFetching", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing UI which allows users to easily revert their choice to
// never save passwords on a certain website.
const base::Feature kRecoverFromNeverSaveAndroid = {
    "RecoverFromNeverSaveAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables considering secondary server field predictions during form parsing.
const base::Feature kSecondaryServerFieldPredictions = {
    "SecondaryServerFieldPredictions", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Displays at least the decryptable and never saved logins in the password
// manager
const base::Feature kSkipUndecryptablePasswords = {
    "SkipUndecryptablePasswords", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the addition of passwords in Chrome Settings.
// TODO(crbug/1226008): Remove once it's launched.
#if BUILDFLAG(IS_IOS)
const base::Feature kSupportForAddPasswordsInSettings = {
    "SupportForAddPasswordsInSettings", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kSupportForAddPasswordsInSettings = {
    "SupportForAddPasswordsInSettings", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_LINUX)
// When enabled, all undecryptable passwords are deleted from the local database
// during initial sync flow.
const base::Feature kSyncUndecryptablePasswordsLinux = {
    "SyncUndecryptablePasswordsLinux", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables the experiment to automatically submit a form after filling by
// TouchToFill
const base::Feature kTouchToFillPasswordSubmission = {
    "TouchToFillPasswordSubmission", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables the intent fetching for the credential manager in Google Mobile
// Services. It does not enable launching the credential manager.
const base::Feature kUnifiedCredentialManagerDryRun = {
    "UnifiedCredentialManagerDryRun", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables use of Google Mobile Services for password storage. Chrome's local
// database will be unused but kept in sync for local passwords.
const base::Feature kUnifiedPasswordManagerAndroid{
    "UnifiedPasswordManagerAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the built-in sync functionality in PasswordSyncBridge becomes
// unused, meaning that SyncService/SyncEngine will no longer download or
// upload changes to/from the Sync server. Instead, an external Android-specific
// backend will be used to achieve similar behavior.
const base::Feature kUnifiedPasswordManagerSyncUsingAndroidBackendOnly{
    "UnifiedPasswordManagerSyncUsingAndroidBackendOnly",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPasswordEditDialogWithDetails{
    "PasswordEditDialogWithDetails", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kUnifiedPasswordManagerDesktop = {
    "UnifiedPasswordManagerDesktop", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support of sending votes on username first flow. The votes are sent
// on single username forms and are based on user interaction with the save
// prompt.
// TODO(crbug.com/959776): Clean up code 2-3 milestones after the launch.
const base::Feature kUsernameFirstFlow = {"UsernameFirstFlow",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support of filling and saving on username first flow.
// TODO(crbug.com/959776): Clean up code 2-3 milestones after the launch.
const base::Feature kUsernameFirstFlowFilling = {
    "UsernameFirstFlowFilling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support of sending additional votes on username first flow. The votes
// are sent on single password forms and contain information about preceding
// single username forms.
const base::Feature kUsernameFirstFlowFallbackCrowdsourcing = {
    "UsernameFirstFlowFallbackCrowdsourcing",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
// Current migration version to Google Mobile Services. If version saved in pref
// is lower than 'kMigrationVersion' passwords will be re-uploaded.
extern const base::FeatureParam<int> kMigrationVersion = {
    &kUnifiedPasswordManagerAndroid, "migration_version", 1};
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

// Enables showing leaked dialog after every successful form submission.
const char kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission[] =
    "should_force_dialog_after_every_sucessful_form_submission";

// Enables showing leaked warning for every site while doing bulk leak check in
// settings.
const char kPasswordChangeInSettingsWithForcedWarningForEverySite[] =
    "should_force_warning_for_every_site_in_settings";

#if BUILDFLAG(IS_ANDROID)
// Enables using conservative heuristics to calculate submission readiness.
const char kTouchToFillPasswordSubmissionWithConservativeHeuristics[] =
    "should_use_conservative_heuristics";
#endif  // IS_ANDROID

bool IsPasswordScriptsFetchingEnabled() {
  return base::FeatureList::IsEnabled(kPasswordScriptsFetching) ||
         base::FeatureList::IsEnabled(kPasswordDomainCapabilitiesFetching);
}

bool IsAutomatedPasswordChangeEnabled() {
  return base::FeatureList::IsEnabled(kPasswordChangeInSettings) ||
         base::FeatureList::IsEnabled(kPasswordChange);
}

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
#endif  // IS_ANDROID

#if BUILDFLAG(IS_ANDROID)
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
#endif  // IS_ANDROID

#if BUILDFLAG(IS_ANDROID)
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

}  // namespace password_manager::features
