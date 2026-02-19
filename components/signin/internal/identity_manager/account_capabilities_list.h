// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate a list of constants. The following line silences a
// presubmit and Tricium warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

// This is the list of account capabilities identifiers and their values. For
// the constant declarations, include the file
// "account_capabilities_constants.h".

// WARNING: Care must be taken to ensure that capabilities are fully available
// server-side before they are added here. This is because (on some platforms)
// if one individual capability fails to evaluate, the fetch will fail for all
// other capabilities as well. To add a capability, please either:
// 1. Wait for the server-side rollout to complete, then add the capability with
//    the ACCOUNT_CAPABILITY() macro.
// 2. Or (if the capability needs to be added to the client before the
//    server-side rollout is complete), add the capability with the
//    ACCOUNT_CAPABILITY_F() macro instead. You must then ensure that the flag
//    is only enabled once the server-side rollout is complete.

// Here we define the values using a macro ACCOUNT_CAPABILITY, so it can be
// expanded differently in some places. The macro has the following signature:
// ACCOUNT_CAPABILITY(cpp_label, java_label, name).

// To define a new account capability that is flag-guarded, add a
// ACCOUNT_CAPABILITY_F(cpp_label, java_label, name, feature_flag) macro
// instead of ACCOUNT_CAPABILITY.
// - The #include for the feature flag must be added to account_capabilities.cc
//   and not to this file

// Here we rely on build_config.h to not introduce any symbols that can be
// textually inserted in the code. If that changes in the future, this header
// should be removed from this file and included transitively instead.
#include "build/build_config.h"

// The server-side documentation and definition for a given capability can be
// found at go/capability-alias (eg. go/capability-alias/ge2dinbnmnqxa).

// clang-format off
// keep-sorted start newline_separated=yes sticky_prefixes=#if group_prefixes=#endif
// clang-format on
ACCOUNT_CAPABILITY(kCanFetchFamilyMemberInfoCapabilityName,
                   CAN_FETCH_FAMILY_MEMBER_INFO_CAPABILITY_NAME,
                   "accountcapabilities/ge2dinbnmnqxa")

#if !BUILDFLAG(IS_IOS)
ACCOUNT_CAPABILITY(kCanHaveEmailAddressDisplayedCapabilityName,
                   CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                   "accountcapabilities/haytqlldmfya")
#endif

#if !BUILDFLAG(IS_ANDROID)
ACCOUNT_CAPABILITY(kCanMakeChromeSearchEngineChoiceScreenChoice,
                   CAN_MAKE_CHROME_SEARCH_ENGINE_CHOICE_SCREEN_CHOICE,
                   "accountcapabilities/ge4tenznmnqxa")
#endif

#if !BUILDFLAG(IS_IOS)
ACCOUNT_CAPABILITY(kCanRunChromePrivacySandboxTrialsCapabilityName,
                   CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                   "accountcapabilities/gu2dqlldmfya")
#endif

ACCOUNT_CAPABILITY(
    kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName,
    CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME,
    "accountcapabilities/gi2tklldmfya")

#if BUILDFLAG(IS_CHROMEOS)
ACCOUNT_CAPABILITY(kCanToggleAutoUpdatesName,
                   CAN_TOGGLE_AUTO_UPDATES_NAME,
                   "accountcapabilities/gu4dmlldmfya")
#endif

#if BUILDFLAG(IS_CHROMEOS)
ACCOUNT_CAPABILITY(kCanUseChromeOSGenerativeAi,
                   CAN_USE_CHROMEOS_GENERATIVE_AI,
                   "accountcapabilities/ge3dgmjnmnqxa")
#endif

#if !BUILDFLAG(IS_IOS)
ACCOUNT_CAPABILITY(kCanUseDevToolsGenerativeAiFeaturesCapabilityName,
                   CAN_USE_DEVTOOLS_GENERATIVE_AI_FEATURES_CAPABILITY_NAME,
                   "accountcapabilities/geztenjnmnqxa")
#endif

#if !BUILDFLAG(IS_IOS)
ACCOUNT_CAPABILITY(kCanUseEduFeaturesCapabilityName,
                   CAN_USE_EDU_FEATURES_CAPABILITY_NAME,
                   "accountcapabilities/gezdsmbnmnqxa")
#endif

ACCOUNT_CAPABILITY(kCanUseGeminiInChromeCapabilityName,
                   CAN_USE_GEMINI_IN_CHROME_CAPABILITY_NAME,
                   "accountcapabilities/giytmnrnmnqxa")

#if BUILDFLAG(IS_CHROMEOS)
ACCOUNT_CAPABILITY(kCanUseGenerativeAiInRecorderApp,
                   CAN_USE_GENERATIVE_AI_IN_RECORDER_APP,
                   "accountcapabilities/ge2tkobnmnqxa")
#endif

#if BUILDFLAG(IS_CHROMEOS)
ACCOUNT_CAPABILITY(kCanUseGenerativeAiPhotoEditing,
                   CAN_USE_GENERATIVE_AI_PHOTO_EDITING,
                   "accountcapabilities/ge3dgobnmnqxa")
#endif

ACCOUNT_CAPABILITY(kCanUseMantaServiceName,
                   CAN_USE_MANTA_SERVICE_NAME,
                   "accountcapabilities/geytcnbnmnqxa")

ACCOUNT_CAPABILITY(kCanUseModelExecutionFeaturesName,
                   CAN_USE_MODEL_EXECUTION_FEATURES_NAME,
                   "accountcapabilities/gezdcnbnmnqxa")

ACCOUNT_CAPABILITY(kCanUseSpeakerLabelInRecorderApp,
                   CAN_USE_SPEAKER_LABEL_IN_RECORDER_APP,
                   "accountcapabilities/ge2tknznmnqxa")

ACCOUNT_CAPABILITY(kIsAllowedForMachineLearningCapabilityName,
                   IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME,
                   "accountcapabilities/g42tslldmfya")

ACCOUNT_CAPABILITY(kIsOptedInToParentalSupervisionCapabilityName,
                   IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME,
                   "accountcapabilities/guzdslldmfya")

ACCOUNT_CAPABILITY(
    kIsSubjectToAccountLevelEnterprisePoliciesCapabilityName,
    IS_SUBJECT_TO_ACCOUNT_LEVEL_ENTERPRISE_POLICIES_CAPABILITY_NAME,
    "accountcapabilities/ge4tgnznmnqxa")

ACCOUNT_CAPABILITY(
    kIsSubjectToChromePrivacySandboxRestrictedMeasurementNotice,
    IS_SUBJECT_TO_CHROME_PRIVACY_SANDBOX_RESTRICTED_MEASUREMENT_NOTICE,
    "accountcapabilities/he4tolldmfya")

ACCOUNT_CAPABILITY(kIsSubjectToEnterprisePoliciesCapabilityName,
                   IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME,
                   "accountcapabilities/g44tilldmfya")

ACCOUNT_CAPABILITY(kIsSubjectToParentalControlsCapabilityName,
                   IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                   "accountcapabilities/guydolldmfya")

#if BUILDFLAG(IS_IOS)
ACCOUNT_CAPABILITY_F(kCanSignInToChromeCapabilityName,
                     CAN_SIGN_IN_TO_CHROME_CAPABILITY_NAME,
                     "accountcapabilities/giztambnmnqxa",
                     switches::kEnforceCanSignInToChromeCapability)
#endif

#if !defined(NDEBUG)
// This is a fake account capability, used for unit tests only.
// To avoid additional fetches in production code, only define this in debug
// builds.
ACCOUNT_CAPABILITY_F(kFakeCapabilityForTestingName,
                     FAKE_CAPABILITY_FOR_TESTING_NAME,
                     "accountcapabilities/fakecapabilityfortesting",
                     switches::kEnableFakeCapabilityForTesting)
#endif

// keep-sorted end
