// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_

// This file defines all the base::FeatureList features for the Password Manager
// module.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/common/password_manager_feature_variations_android.h"
#endif

namespace password_manager::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kBiometricTouchToFill;
extern const base::Feature kDetectFormSubmissionOnFormClear;
extern const base::Feature kEnableFaviconForPasswords;
extern const base::Feature kEnableManualPasswordGeneration;
extern const base::Feature kEnableOverwritingPlaceholderUsernames;
extern const base::Feature kEnablePasswordsAccountStorage;
extern const base::Feature KEnablePasswordGenerationForClearTextFields;
extern const base::Feature kFillingAcrossAffiliatedWebsites;
extern const base::Feature kFillOnAccountSelect;
#if BUILDFLAG(IS_LINUX)
extern const base::Feature kForceInitialSyncWhenDecryptionFails;
#endif
extern const base::Feature kInferConfirmationPasswordField;
extern const base::Feature kIOSEnablePasswordManagerBrandingUpdate;
extern const base::Feature kMuteCompromisedPasswords;
extern const base::Feature kPasswordNotes;
extern const base::Feature kSendPasswords;
extern const base::Feature kLeakDetectionUnauthenticated;
extern const base::Feature kPasswordChange;
extern const base::Feature kPasswordChangeOnlyRecentCredentials;
extern const base::Feature kPasswordChangeInSettings;
extern const base::Feature kPasswordDomainCapabilitiesFetching;
extern const base::Feature kPasswordImport;
extern const base::Feature kPasswordReuseDetectionEnabled;
extern const base::Feature kPasswordsAccountStorageRevisedOptInFlow;
extern const base::Feature kPasswordScriptsFetching;
extern const base::Feature kRecoverFromNeverSaveAndroid;
extern const base::Feature kReparseServerPredictionsFollowingFormChange;
extern const base::Feature kSecondaryServerFieldPredictions;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
extern const base::Feature kSkipUndecryptablePasswords;
#endif
extern const base::Feature kSupportForAddPasswordsInSettings;
#if BUILDFLAG(IS_LINUX)
extern const base::Feature kSyncUndecryptablePasswordsLinux;
#endif
#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kTouchToFillPasswordSubmission;
#endif
extern const base::Feature kTreatNewPasswordHeuristicsAsReliable;
#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kUnifiedCredentialManagerDryRun;
extern const base::Feature kUnifiedPasswordManagerAndroid;
extern const base::Feature kUnifiedPasswordManagerMigration;
extern const base::Feature kUnifiedPasswordManagerShadowAndroid;
extern const base::Feature kUnifiedPasswordManagerShadowWriteOperationsAndroid;
extern const base::Feature kUnifiedPasswordManagerSyncUsingAndroidBackendOnly;
#endif
extern const base::Feature kUnifiedPasswordManagerDesktop;
extern const base::Feature kUsernameFirstFlow;
extern const base::Feature kUsernameFirstFlowFilling;
extern const base::Feature kUsernameFirstFlowFallbackCrowdsourcing;

// All features parameters are in alphabetical order.
extern const base::FeatureParam<bool> kPasswordChangeLiveExperimentParam;
#if BUILDFLAG(IS_ANDROID)
extern const base::FeatureParam<int> kMigrationVersion;
constexpr base::FeatureParam<UpmExperimentVariation>::Option
    kUpmExperimentVariationOption[] = {
        {UpmExperimentVariation::kEnableForSyncingUsers, "0"},
        {UpmExperimentVariation::kShadowSyncingUsers, "1"}};

constexpr base::FeatureParam<UpmExperimentVariation>
    kUpmExperimentVariationParam{&kUnifiedPasswordManagerAndroid, "stage",
                                 UpmExperimentVariation::kEnableForSyncingUsers,
                                 &kUpmExperimentVariationOption};

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

#if BUILDFLAG(IS_ANDROID)
// Returns true if the unified password manager feature is active and in a stage
// that allows to use the new UI.
bool UsesUnifiedPasswordManagerUi();
#endif  // IS_ANDROID

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
