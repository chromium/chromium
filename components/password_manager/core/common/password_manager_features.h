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
BASE_DECLARE_FEATURE(kDisablePasswordsDropdownForCvcFields);
BASE_DECLARE_FEATURE(kEnableOverwritingPlaceholderUsernames);

BASE_DECLARE_FEATURE(kEnablePasswordsAccountStorage);
inline constexpr base::FeatureParam<int>
    kMaxAccountStorageNewFeatureIconImpressions = {
        &kEnablePasswordsAccountStorage,
        "max_account_storage_new_feature_icon_impressions", 5};

BASE_DECLARE_FEATURE(kEnablePasswordGenerationForClearTextFields);
BASE_DECLARE_FEATURE(kEnablePasswordManagerWithinFencedFrame);
BASE_DECLARE_FEATURE(kFillingAcrossAffiliatedWebsites);
BASE_DECLARE_FEATURE(kFillingAcrossGroupedSites);
BASE_DECLARE_FEATURE(kFillOnAccountSelect);
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kForceInitialSyncWhenDecryptionFails);
#endif
BASE_DECLARE_FEATURE(kInferConfirmationPasswordField);
#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kIOSPasswordUISplit);
BASE_DECLARE_FEATURE(kIOSPasswordCheckup);
BASE_DECLARE_FEATURE(kIOSPasswordBottomSheet);
#endif                                            // IS_IOS
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kMemoryMapWeaknessCheckDictionaries);
#endif
BASE_DECLARE_FEATURE(kNewRegexForOtpFields);
BASE_DECLARE_FEATURE(kPasswordIssuesInSpecificsMetadata);
BASE_DECLARE_FEATURE(kSendPasswords);
BASE_DECLARE_FEATURE(kPasswordChangeWellKnown);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kPasswordManagerRedesign);
#endif
BASE_DECLARE_FEATURE(kPasswordReuseDetectionEnabled);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kPasswordGenerationExperiment);
#endif
BASE_DECLARE_FEATURE(kPasswordsGrouping);
BASE_DECLARE_FEATURE(kPasswordsImportM2);
BASE_DECLARE_FEATURE(kPasswordStrengthIndicator);
BASE_DECLARE_FEATURE(kRecoverFromNeverSaveAndroid);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kRevampedPasswordManagementBubble);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);
#endif
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kPasskeyManagementUsingAccountSettingsAndroid);
BASE_DECLARE_FEATURE(kPasswordEditDialogWithDetails);
BASE_DECLARE_FEATURE(kPasswordGenerationBottomSheet);
BASE_DECLARE_FEATURE(kUnifiedCredentialManagerDryRun);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerAndroid);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroid);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerSyncUsingAndroidBackendOnly);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerAndroidBranding);
BASE_DECLARE_FEATURE(kExploratorySaveUpdatePasswordStrings);
BASE_DECLARE_FEATURE(kPasswordsInCredMan);
#endif
BASE_DECLARE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing);
BASE_DECLARE_FEATURE(kUsernameFirstFlowHonorAutocomplete);
BASE_DECLARE_FEATURE(kPasswordGenerationPreviewOnHover);

// All features parameters are in alphabetical order.

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// This enum supports enabling specific arms of the
// `kPasswordGenerationExperiment` (go/strong-passwords-desktop).
// Keep the order consistent with
// `kPasswordGenerationExperimentVariationOption` below and with
// `kPasswordGenerationExperimentVariations` in about_flags.cc.
enum class PasswordGenerationVariation {
  // Adjusts the language focusing on recommendation and security messaging.
  kTrustedAdvice = 1,
  // Adjusts the language making the suggestion softer and more guiding.
  kSafetyFirst = 2,
  // Adjusts the language adding a more persuasive and reassuring tone.
  kTrySomethingNew = 3,
  // Adjusts the language focusing on the convenience of use.
  kConvenience = 4,
};

inline constexpr base::FeatureParam<PasswordGenerationVariation>::Option
    kPasswordGenerationExperimentVariationOption[] = {
        {PasswordGenerationVariation::kTrustedAdvice, "trusted_advice"},
        {PasswordGenerationVariation::kSafetyFirst, "safety_first"},
        {PasswordGenerationVariation::kTrySomethingNew, "try_something_new"},
        {PasswordGenerationVariation::kConvenience, "convenience"},
};

inline constexpr base::FeatureParam<PasswordGenerationVariation>
    kPasswordGenerationExperimentVariationParam{
        &kPasswordGenerationExperiment, "password_generation_variation",
        PasswordGenerationVariation::kTrustedAdvice,
        &kPasswordGenerationExperimentVariationOption};
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// If true, then password strength indicator will display a minimized state for
// passwords with more than 5 characters as long as they are weak. Otherwise,
// the full dropdown will be displayed as long as the password is weak.
inline constexpr base::FeatureParam<bool>
    kPasswordStrengthIndicatorWithMinimizedState = {
        &kPasswordStrengthIndicator, "strength_indicator_minimized", false};

#if BUILDFLAG(IS_ANDROID)

// Current list of the GMS Core API error codes that should be ignored and not
// result in user eviction.
// Errors to ignore: AUTH_ERROR_RESOLVABLE, AUTH_ERROR_UNRESOLVABLE
inline constexpr base::FeatureParam<std::string> kIgnoredGmsApiErrors = {
    &kUnifiedPasswordManagerAndroid, "ignored_api_errors", "11005,11006"};

// Current list of the GMS Core API error codes considered retriable.
// User could still be evicted if retries do not resolve the error.
// Retriable errors: NETWORK_ERROR, API_NOT_CONNECTED,
// CONNECTION_SUSPENDED_DURING_CALL, RECONNECTION_TIMED_OUT,
// BACKEND_GENERIC
inline constexpr base::FeatureParam<std::string> kRetriableGmsApiErrors = {
    &kUnifiedPasswordManagerAndroid, "retriable_api_errors",
    "7,17,20,22,11009"};

inline constexpr base::FeatureParam<UpmExperimentVariation>::Option
    kUpmExperimentVariationOption[] = {
        {UpmExperimentVariation::kEnableForSyncingUsers, "0"},
        {UpmExperimentVariation::kShadowSyncingUsers, "1"},
        {UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers, "2"},
        {UpmExperimentVariation::kEnableForAllUsers, "3"},
};

inline constexpr base::FeatureParam<UpmExperimentVariation>
    kUpmExperimentVariationParam{&kUnifiedPasswordManagerAndroid, "stage",
                                 UpmExperimentVariation::kEnableForSyncingUsers,
                                 &kUpmExperimentVariationOption};

extern const base::FeatureParam<int> kSaveUpdatePromptSyncingStringVersion;
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

#if BUILDFLAG(IS_ANDROID)
// Touch To Fill submission feature's variations.
extern const char kTouchToFillPasswordSubmissionWithConservativeHeuristics[];
#endif  // IS_ANDROID

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
#endif  // IS_ANDROID

#if BUILDFLAG(IS_IOS)
// Returns true if the Password Checkup feature flag is enabled.
bool IsPasswordCheckupEnabled();
#endif  // IS_IOS

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
