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
extern const base::Feature kBiometricAuthenticationForFilling;
#endif
#if BUILDFLAG(IS_MAC)
extern const base::Feature kBiometricAuthenticationInSettings;
#endif
extern const base::Feature kBiometricTouchToFill;
extern const base::Feature kDetectFormSubmissionOnFormClear;
extern const base::Feature kForceEnablePasswordDomainCapabilities;
extern const base::Feature kEnableFaviconForPasswords;
extern const base::Feature kEnableOverwritingPlaceholderUsernames;
extern const base::Feature kEnablePasswordsAccountStorage;
extern const base::Feature KEnablePasswordGenerationForClearTextFields;
extern const base::Feature kEnablePasswordManagerWithinFencedFrame;
extern const base::Feature kFillingAcrossAffiliatedWebsites;
extern const base::Feature kFillOnAccountSelect;
#if BUILDFLAG(IS_LINUX)
extern const base::Feature kForceInitialSyncWhenDecryptionFails;
#endif
extern const base::Feature kInferConfirmationPasswordField;
extern const base::Feature kIOSEnablePasswordManagerBrandingUpdate;
#if BUILDFLAG(IS_IOS)
extern const base::Feature kIOSPasswordUISplit;
extern const base::Feature kIOSPasswordManagerCrossOriginIframeSupport;
#endif  // IS_IOS
extern const base::Feature kMuteCompromisedPasswords;

extern const base::FeatureParam<base::TimeDelta> kPasswordNotesAuthValidity;
extern const base::Feature kPasswordNotes;

extern const base::Feature kPasswordViewPageInSettings;
extern const base::Feature kSendPasswords;
extern const base::Feature kLeakDetectionUnauthenticated;
extern const base::Feature kPasswordChange;
extern const base::Feature kPasswordChangeInSettings;
extern const base::Feature kPasswordChangeWellKnown;
extern const base::Feature kPasswordDomainCapabilitiesFetching;
extern const base::Feature kPasswordImport;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
extern const base::Feature kPasswordManagerRedesign;
#endif
extern const base::Feature kPasswordReuseDetectionEnabled;
extern const base::Feature kPasswordScriptsFetching;
extern const base::Feature kPasswordsGrouping;
extern const base::Feature kPasswordStrengthIndicator;
extern const base::Feature kRecoverFromNeverSaveAndroid;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
extern const base::Feature kSkipUndecryptablePasswords;
#endif
#if BUILDFLAG(IS_LINUX)
extern const base::Feature kSyncUndecryptablePasswordsLinux;
#endif
#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kPasswordEditDialogWithDetails;
extern const base::Feature kShowUPMErrorNotification;
extern const base::Feature kTouchToFillPasswordSubmission;
extern const base::Feature kUnifiedCredentialManagerDryRun;
extern const base::Feature kUnifiedPasswordManagerAndroid;
extern const base::Feature kUnifiedPasswordManagerErrorMessages;
extern const base::Feature kUnifiedPasswordManagerSyncUsingAndroidBackendOnly;
extern const base::Feature kUnifiedPasswordManagerReenrollment;
#endif
extern const base::Feature kUsernameFirstFlowFallbackCrowdsourcing;

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
// that allows to use the new UI.
bool UsesUnifiedPasswordManagerUi();
#endif  // IS_ANDROID

#if BUILDFLAG(IS_ANDROID)
// Returns true if the unified password manager feature is active and in a stage
// that requires migrating existing credentials. Independent of
// whether only non-syncable data needs to be migrated or full credentials.
bool RequiresMigrationForUnifiedPasswordManager();
#endif  // IS_ANDROID

#if BUILDFLAG(IS_ANDROID)
// Returns true if the unified password manager feature is active and in a stage
// that uses the unified storage for passwords that remain local on the device.
bool ManagesLocalPasswordsInUnifiedPasswordManager();
#endif  // IS_ANDROID

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
