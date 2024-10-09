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

// Here we define the values using a macro ACCOUNT_CAPABILITY, so it can be
// expanded differently in some places. The macro has the following signature:
// ACCOUNT_CAPABILITY(cpp_label, java_label, name).

// Please keep the list alphabetically sorted by the capability identifiers.

ACCOUNT_CAPABILITY(kCanHaveEmailAddressDisplayedCapabilityName,
                   CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                   "accountcapabilities/haytqlldmfya")

ACCOUNT_CAPABILITY(
    kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName,
    CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME,
    "accountcapabilities/gi2tklldmfya")

ACCOUNT_CAPABILITY(kCanRunChromePrivacySandboxTrialsCapabilityName,
                   CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                   "accountcapabilities/gu2dqlldmfya")

ACCOUNT_CAPABILITY(kIsOptedInToParentalSupervisionCapabilityName,
                   IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME,
                   "accountcapabilities/guzdslldmfya")

ACCOUNT_CAPABILITY(kCanFetchFamilyMemberInfoCapabilityName,
                   CAN_FETCH_FAMILY_MEMBER_INFO_CAPABILITY_NAME,
                   "accountcapabilities/ge2dinbnmnqxa")

ACCOUNT_CAPABILITY(kCanToggleAutoUpdatesName,
                   CAN_TOGGLE_AUTO_UPDATES_NAME,
                   "accountcapabilities/gu4dmlldmfya")

ACCOUNT_CAPABILITY(kCanUseChromeIpProtectionName,
                   CAN_USE_CHROME_IP_PROTECTION_NAME,
                   "accountcapabilities/geydgnznmnqxa")

ACCOUNT_CAPABILITY(kCanUseCopyEditorFeatureName,
                   CAN_USE_COPYEDITOR_FEATURE_NAME,
                   "accountcapabilities/ge2tkmznmnqxa")

ACCOUNT_CAPABILITY(kCanUseDevToolsGenerativeAiFeaturesCapabilityName,
                   CAN_USE_DEVTOOLS_GENERATIVE_AI_FEATURES_CAPABILITY_NAME,
                   "accountcapabilities/geztenjnmnqxa")

ACCOUNT_CAPABILITY(kCanUseMantaServiceName,
                   CAN_USE_MANTA_SERVICE_NAME,
                   "accountcapabilities/geytcnbnmnqxa")

ACCOUNT_CAPABILITY(kCanUseModelExecutionFeaturesName,
                   CAN_USE_MODEL_EXECUTION_FEATURES_NAME,
                   "accountcapabilities/gezdcnbnmnqxa")

ACCOUNT_CAPABILITY(kIsAllowedForMachineLearningCapabilityName,
                   IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME,
                   "accountcapabilities/g42tslldmfya")

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

ACCOUNT_CAPABILITY(kCanUseEduFeaturesCapabilityName,
                   CAN_USE_EDU_FEATURES_CAPABILITY_NAME,
                   "accountcapabilities/gezdsmbnmnqxa")

ACCOUNT_CAPABILITY(kCanUseSpeakerLabelInRecorderApp,
                   CAN_USE_SPEAKER_LABEL_IN_RECORDER_APP,
                   "accountcapabilities/ge2tknznmnqxa")

ACCOUNT_CAPABILITY(kCanUseGenerativeAiInRecorderApp,
                   CAN_USE_GENERATIVE_AI_IN_RECORDER_APP,
                   "accountcapabilities/ge2tkobnmnqxa")
