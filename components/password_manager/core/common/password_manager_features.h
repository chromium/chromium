// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_

// This file defines all the base::FeatureList features for the Password Manager
// module.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/common/password_manager_feature_variations_android.h"
#endif

namespace password_manager::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kBiometricAuthenticationForFilling);
#endif
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kBiometricAuthenticationInSettings);
#endif
BASE_DECLARE_FEATURE(kBiometricTouchToFill);
BASE_DECLARE_FEATURE(kDetectFormSubmissionOnFormClear);
BASE_DECLARE_FEATURE(kForceEnablePasswordDomainCapabilities);
BASE_DECLARE_FEATURE(kEnableFaviconForPasswords);
BASE_DECLARE_FEATURE(kEnableOverwritingPlaceholderUsernames);
BASE_DECLARE_FEATURE(kEnablePasswordsAccountStorage);
BASE_DECLARE_FEATURE(kEnablePasswordGenerationForClearTextFields);
BASE_DECLARE_FEATURE(kEnablePasswordManagerWithinFencedFrame);
BASE_DECLARE_FEATURE(kFillingAcrossAffiliatedWebsites);
BASE_DECLARE_FEATURE(kFillOnAccountSelect);
#if BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kForceInitialSyncWhenDecryptionFails);
#endif
BASE_DECLARE_FEATURE(kInferConfirmationPasswordField);
BASE_DECLARE_FEATURE(kIOSEnablePasswordManagerBrandingUpdate);
#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kIOSPasswordUISplit);
BASE_DECLARE_FEATURE(kIOSPasswordManagerCrossOriginIframeSupport);
#endif  // IS_IOS
BASE_DECLARE_FEATURE(kMuteCompromisedPasswords);

BASE_DECLARE_FEATURE(kPasswordViewPageInSettings);
BASE_DECLARE_FEATURE(kSendPasswords);
BASE_DECLARE_FEATURE(kLeakDetectionUnauthenticated);
BASE_DECLARE_FEATURE(kPasswordChange);
BASE_DECLARE_FEATURE(kPasswordChangeInSettings);
BASE_DECLARE_FEATURE(kPasswordChangeWellKnown);
BASE_DECLARE_FEATURE(kPasswordDomainCapabilitiesFetching);
BASE_DECLARE_FEATURE(kPasswordImport);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kPasswordManagerRedesign);
#endif
BASE_DECLARE_FEATURE(kPasswordReuseDetectionEnabled);
BASE_DECLARE_FEATURE(kPasswordScriptsFetching);
BASE_DECLARE_FEATURE(kPasswordsGrouping);
BASE_DECLARE_FEATURE(kPasswordStrengthIndicator);
BASE_DECLARE_FEATURE(kRecoverFromNeverSaveAndroid);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);
#endif
#if BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kSyncUndecryptablePasswordsLinux);
#endif
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kPasswordEditDialogWithDetails);
BASE_DECLARE_FEATURE(kShowUPMErrorNotification);
BASE_DECLARE_FEATURE(kTouchToFillPasswordSubmission);
BASE_DECLARE_FEATURE(kUnifiedCredentialManagerDryRun);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerAndroid);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerErrorMessages);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerSyncUsingAndroidBackendOnly);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerReenrollment);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerAndroidBranding);
#endif
BASE_DECLARE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing);

// All features parameters are in alphabetical order.

// If `true`, then password change in settings will also be offered for
// insecure credentials that are weak (and not phished or leaked).
constexpr base::FeatureParam<bool>
    kPasswordChangeInSettingsWeakCredentialsParam = {&kPasswordChangeInSettings,
                                                     "weak_credentials", false};

// True if the client is part of the live_experiment group for
// |kPasswordDomainCapabilitiesFetching|, otherwise, the client is assumed to be
// in the regular launch group.
constexpr base::FeatureParam<bool> kPasswordChangeLiveExperimentParam = {
    &kPasswordDomainCapabilitiesFetching, "live_experiment", false};

#if BUILDFLAG(IS_ANDROID)
extern const base::FeatureParam<int> kMigrationVersion;

// Current version of the GMS Core API errors lists. Users save this value on
// eviction due to error and will only be re-enrolled to the experiment if the
// configured version is greater than the saved one.
constexpr base::FeatureParam<int> kGmsApiErrorListVersion = {
    &kUnifiedPasswordManagerAndroid, "api_error_list_version", 0};

// Current list of the GMS Core API error codes that should be ignored and not
// result in user eviction.
// Codes DEVELOPER_ERROR=10, BAD_REQUEST=11008 are ignored to keep the default
// pre-M107 behaviour.
constexpr base::FeatureParam<std::string> kIgnoredGmsApiErrors = {
    &kUnifiedPasswordManagerAndroid, "ignored_api_errors", "10,11008"};

// Current list of the GMS Core API error codes considered retriable.
// User could still be evicted if retries do not resolve the error.
constexpr base::FeatureParam<std::string> kRetriableGmsApiErrors = {
    &kUnifiedPasswordManagerAndroid, "retriable_api_errors", ""};

// Enables fallback to the Chrome built-in backend if the operation executed on
// the GMS Core backend returns with error. Errors listed in the
// |kIgnoredGmsApiErrors| will not fallback and will be directly returned to the
// caller to be addressed in a specific way.

// Fallback on AddLogin and UpdateLogin operations. This is default behaviour
// since M103.
constexpr base::FeatureParam<bool> kFallbackOnModifyingOperations = {
    &kUnifiedPasswordManagerAndroid, "fallback_on_modifying_operations", true};

// Fallback on RemoveLogin* operations.
constexpr base::FeatureParam<bool> kFallbackOnRemoveOperations = {
    &kUnifiedPasswordManagerAndroid, "fallback_on_remove_operations", false};

// Fallback on FillMatchingLogins which is needed to perform autofill and could
// affect user experience.
constexpr base::FeatureParam<bool> kFallbackOnUserAffectingReadOperations = {
    &kUnifiedPasswordManagerAndroid,
    "fallback_on_user_affecting_read_operations", false};

// Fallback on GetAllLogins* and GetAutofillableLogins operations which are
// needed for certain features (e.g. PhishGuard) but do not affect the core
// experience.
constexpr base::FeatureParam<bool> kFallbackOnNonUserAffectingReadOperations = {
    &kUnifiedPasswordManagerAndroid,
    "fallback_on_non_user_affecting_read_operations", false};

constexpr base::FeatureParam<UpmExperimentVariation>::Option
    kUpmExperimentVariationOption[] = {
        {UpmExperimentVariation::kEnableForSyncingUsers, "0"},
        {UpmExperimentVariation::kShadowSyncingUsers, "1"},
        {UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers, "2"},
        {UpmExperimentVariation::kEnableForAllUsers, "3"},
};

constexpr base::FeatureParam<UpmExperimentVariation>
    kUpmExperimentVariationParam{&kUnifiedPasswordManagerAndroid, "stage",
                                 UpmExperimentVariation::kEnableForSyncingUsers,
                                 &kUpmExperimentVariationOption};

extern const base::FeatureParam<int> kMaxUPMReenrollments;
extern const base::FeatureParam<int> kMaxUPMReenrollmentAttempts;

extern const base::FeatureParam<bool> kIgnoreAuthErrorMessageTimeouts;
extern const base::FeatureParam<int> kMaxShownUPMErrorsBeforeEviction;
#endif

// Field trial and corresponding parameters.
// To manually override this, start Chrome with the following parameters:
//   --enable-features=PasswordGenerationRequirements,\
//       PasswordGenerationRequirementsDomainOverrides
//   --force-fieldtrials=PasswordGenerationRequirements/Enabled
//   --force-fieldtrial-params=PasswordGenerationRequirements.Enabled:\
//       version/0/prefix_length/0/timeout/5000
extern const char kGenerationRequirementsFieldTrial[];
extern const char kGenerationRequirementsVersion[];
extern const char kGenerationRequirementsPrefixLength[];
extern const char kGenerationRequirementsTimeout[];

// Password change feature variations.
extern const char
    kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission[];
extern const char kPasswordChangeInSettingsWithForcedWarningForEverySite[];

#if BUILDFLAG(IS_ANDROID)
// Touch To Fill submission feature's variations.
extern const char kTouchToFillPasswordSubmissionWithConservativeHeuristics[];
#endif  // IS_ANDROID

// Returns true if any of the password script fetching related flags are
// enabled.
bool IsPasswordScriptsFetchingEnabled();

// Returns true if any of the features that unlock entry points for password
// change flows are enabled.
bool IsAutomatedPasswordChangeEnabled();

#if BUILDFLAG(IS_ANDROID)
// Returns true if the unified password manager feature is active and in a stage
// that allows to use the new feature end-to-end.
bool UsesUnifiedPasswordManagerUi();

// Returns true when unified password manager strings & icons should be
// displayed. It provides the option to enable the UPM branding UI earlier then
// the UPM feature itself.
bool UsesUnifiedPasswordManagerBranding();

// Returns true if the unified password manager feature is active and in a stage
// that requires migrating existing credentials. Independent of
// whether only non-syncable data needs to be migrated or full credentials.
bool RequiresMigrationForUnifiedPasswordManager();

// Returns true if the unified password manager feature is active and in a stage
// that uses the unified storage for passwords that remain local on the device.
bool ManagesLocalPasswordsInUnifiedPasswordManager();
#endif  // IS_ANDROID

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
