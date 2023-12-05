// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_

// This file defines all password manager features used in the browser process.
// Prefer adding new features here instead of "core/common/".
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace password_manager::features {
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

BASE_DECLARE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender);
BASE_DECLARE_FEATURE(kBiometricTouchToFill);
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);
BASE_DECLARE_FEATURE(kDisablePasswordsDropdownForCvcFields);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kRemoveUPMUnenrollment);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kEnablePasswordsAccountStorage);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kFillingAcrossAffiliatedWebsitesAndroid);
BASE_DECLARE_FEATURE(kFetchGaiaHashOnSignIn);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kFillingAcrossGroupedSites);
BASE_DECLARE_FEATURE(kFillOnAccountSelect);

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kIOSPasswordSignInUff);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kNewConfirmationBubbleForGeneratedPasswords);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Have GPM passkeys trigger prefetching affiliation like passwords do.
BASE_DECLARE_FEATURE(kPasskeysPrefetchAffiliations);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kPasswordGenerationExperiment);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_DECLARE_FEATURE(kPasswordManagerEnableReceiverService);
BASE_DECLARE_FEATURE(kPasswordManagerEnableSenderService);
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC)

BASE_DECLARE_FEATURE(kSharedPasswordNotificationUI);
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
#endif  // !BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kUseExtensionListForPSLMatching);
BASE_DECLARE_FEATURE(kUseServerPredictionsOnSaveParsing);
BASE_DECLARE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing);
BASE_DECLARE_FEATURE(kUsernameFirstFlowHonorAutocomplete);

BASE_DECLARE_FEATURE(kUsernameFirstFlowStoreSeveralValues);
// If `kUsernameFirstFlowStoreSeveralValues` is enabled, the size of LRU
// cache that stores all username candidates outside the form.
extern const base::FeatureParam<int> kMaxSingleUsernameFieldsToStore;

BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValues);
// If `kUsernameFirstFlowWithIntermediateValues` is enabled, after this amount
// of minutes single username will not be used in the save prompt.
extern const base::FeatureParam<int> kSingleUsernameTimeToLive;
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValuesPredictions);
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValuesVoting);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kUseGMSCoreForBrandingInfo);
#endif

// All features parameters in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
// Minimum GMSCore version required to remove unenrollment. Setting version
// lower than the default one will have no effect.
inline constexpr base::FeatureParam<int>
    kMinimumGMSCoreVersionToRemoveUnenrollment{
        &kRemoveUPMUnenrollment, "min_gms_core_version", 225012000};
#endif

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
  // Adjusts the language of the help text pointing out the benefits.
  kCrossDevice = 5,
  // Adds a row for switching to editing the suggested password directly.
  kEditPassword = 6,
  // Adds chunking generated passwords into smaller readable parts.
  kChunkPassword = 7,
  // Removes strong password row and adds nudge passwords buttons instead.
  kNudgePassword = 8,
};

inline constexpr base::FeatureParam<PasswordGenerationVariation>::Option
    kPasswordGenerationExperimentVariationOption[] = {
        {PasswordGenerationVariation::kTrustedAdvice, "trusted_advice"},
        {PasswordGenerationVariation::kSafetyFirst, "safety_first"},
        {PasswordGenerationVariation::kTrySomethingNew, "try_something_new"},
        {PasswordGenerationVariation::kConvenience, "convenience"},
        {PasswordGenerationVariation::kCrossDevice, "cross_device"},
        {PasswordGenerationVariation::kEditPassword, "edit_password"},
        {PasswordGenerationVariation::kChunkPassword, "chunk_password"},
        {PasswordGenerationVariation::kNudgePassword, "nudge_password"},
};

inline constexpr base::FeatureParam<PasswordGenerationVariation>
    kPasswordGenerationExperimentVariationParam{
        &kPasswordGenerationExperiment, "password_generation_variation",
        PasswordGenerationVariation::kTrustedAdvice,
        &kPasswordGenerationExperimentVariationOption};

inline constexpr base::FeatureParam<std::string>
    kPasswordGenerationExperimentSurveyTriggerId{
        &kPasswordGenerationExperiment,
        "PasswordGenerationExperimentSurveyTriggedId", /*default_value=*/""};

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
