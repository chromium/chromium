// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"

#include "components/signin/internal/identity_manager/account_capabilities_constants.h"

AccountCapabilitiesTestMutator::AccountCapabilitiesTestMutator(
    AccountCapabilities* capabilities)
    : capabilities_(capabilities) {}

// static
const std::vector<std::string>&
AccountCapabilitiesTestMutator::GetSupportedAccountCapabilityNames() {
  return AccountCapabilities::GetSupportedAccountCapabilityNames();
}

void AccountCapabilitiesTestMutator::set_can_fetch_family_member_info(
    bool value) {
  capabilities_->capabilities_map_[kCanFetchFamilyMemberInfoCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_have_email_address_displayed(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanHaveEmailAddressDisplayedCapabilityName] = value;
}

void AccountCapabilitiesTestMutator::
    set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        bool value) {
  capabilities_->capabilities_map_
      [kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_run_chrome_privacy_sandbox_trials(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanRunChromePrivacySandboxTrialsCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_is_opted_in_to_parental_supervision(
    bool value) {
  capabilities_
      ->capabilities_map_[kIsOptedInToParentalSupervisionCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_toggle_auto_updates(bool value) {
  capabilities_->capabilities_map_[kCanToggleAutoUpdatesName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_chrome_ip_protection(
    bool value) {
  capabilities_->capabilities_map_[kCanUseChromeIpProtectionName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_copyeditor_feature(
    bool value) {
  capabilities_->capabilities_map_[kCanUseCopyEditorFeatureName] = value;
}

void AccountCapabilitiesTestMutator::
    set_can_use_devtools_generative_ai_features(bool value) {
  capabilities_
      ->capabilities_map_[kCanUseDevToolsGenerativeAiFeaturesCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_use_edu_features(bool value) {
  capabilities_->capabilities_map_[kCanUseEduFeaturesCapabilityName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_manta_service(bool value) {
  capabilities_->capabilities_map_[kCanUseMantaServiceName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_model_execution_features(
    bool value) {
  capabilities_->capabilities_map_[kCanUseModelExecutionFeaturesName] = value;
}

void AccountCapabilitiesTestMutator::set_is_allowed_for_machine_learning(
    bool value) {
  capabilities_->capabilities_map_[kIsAllowedForMachineLearningCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::
    set_is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(
        bool value) {
  capabilities_->capabilities_map_
      [kIsSubjectToChromePrivacySandboxRestrictedMeasurementNotice] = value;
}

void AccountCapabilitiesTestMutator::set_is_subject_to_enterprise_policies(
    bool value) {
  capabilities_
      ->capabilities_map_[kIsSubjectToEnterprisePoliciesCapabilityName] = value;
}

void AccountCapabilitiesTestMutator::set_is_subject_to_parental_controls(
    bool value) {
  capabilities_->capabilities_map_[kIsSubjectToParentalControlsCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_use_speaker_label_in_recorder_app(
    bool value) {
  capabilities_->capabilities_map_[kCanUseSpeakerLabelInRecorderApp] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_generative_ai_in_recorder_app(
    bool value) {
  capabilities_->capabilities_map_[kCanUseGenerativeAiInRecorderApp] = value;
}

void AccountCapabilitiesTestMutator::SetAllSupportedCapabilities(bool value) {
  for (const std::string& name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    capabilities_->capabilities_map_[name] = value;
  }
}

void AccountCapabilitiesTestMutator::SetCapability(const std::string& name,
                                                   bool value) {
  const std::vector<std::string>& capability_names =
      AccountCapabilities::GetSupportedAccountCapabilityNames();
  CHECK(std::find(capability_names.begin(), capability_names.end(), name) !=
        capability_names.end())
      << "Invalid capability name: " << name;
  capabilities_->capabilities_map_[name] = value;
}
