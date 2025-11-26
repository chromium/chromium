// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"

#include <ostream>

#include "base/check.h"
#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"

AccountCapabilitiesTestMutator::AccountCapabilitiesTestMutator(
    AccountCapabilities* capabilities)
    : capabilities_(capabilities) {}

// static
base::span<const std::string_view>
AccountCapabilitiesTestMutator::GetSupportedAccountCapabilityNames() {
  return AccountCapabilities::GetSupportedAccountCapabilityNames();
}

// clang-format off
// keep-sorted start newline_separated=yes sticky_prefixes=#if group_prefixes=#endif block=yes
// clang-format on
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

#if !BUILDFLAG(IS_ANDROID)
void AccountCapabilitiesTestMutator::set_can_make_chrome_search_engine_choice_screen_choice(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanMakeChromeSearchEngineChoiceScreenChoice] =
      value;
}
#endif

void AccountCapabilitiesTestMutator::set_can_run_chrome_privacy_sandbox_trials(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanRunChromePrivacySandboxTrialsCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::
    set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        bool value) {
  capabilities_->capabilities_map_
      [kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_toggle_auto_updates(bool value) {
  capabilities_->capabilities_map_[kCanToggleAutoUpdatesName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_chrome_ip_protection(
    bool value) {
  capabilities_->capabilities_map_[kCanUseChromeIpProtectionName] = value;
}

#if BUILDFLAG(IS_CHROMEOS)
void AccountCapabilitiesTestMutator::set_can_use_chromeos_generative_ai(
    bool value) {
  capabilities_->capabilities_map_[kCanUseChromeOSGenerativeAi] = value;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void AccountCapabilitiesTestMutator::set_can_use_gemini_in_chrome(bool value) {
  // TODO(crbug.com/462697239): The current implementation is a placeholder to
  // unblock development. Update this with the account capability once it is
  // available from the server.
  capabilities_->capabilities_map_[kIsSubjectToParentalControlsCapabilityName] =
      !value;
}
#endif

void AccountCapabilitiesTestMutator::set_can_use_generative_ai_in_recorder_app(
    bool value) {
  capabilities_->capabilities_map_[kCanUseGenerativeAiInRecorderApp] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_generative_ai_photo_editing(
    bool value) {
  capabilities_->capabilities_map_[kCanUseGenerativeAiPhotoEditing] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_manta_service(bool value) {
  capabilities_->capabilities_map_[kCanUseMantaServiceName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_model_execution_features(
    bool value) {
  capabilities_->capabilities_map_[kCanUseModelExecutionFeaturesName] = value;
}

void AccountCapabilitiesTestMutator::set_can_use_speaker_label_in_recorder_app(
    bool value) {
  capabilities_->capabilities_map_[kCanUseSpeakerLabelInRecorderApp] = value;
}

void AccountCapabilitiesTestMutator::set_is_allowed_for_machine_learning(
    bool value) {
  capabilities_->capabilities_map_[kIsAllowedForMachineLearningCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_is_opted_in_to_parental_supervision(
    bool value) {
  capabilities_
      ->capabilities_map_[kIsOptedInToParentalSupervisionCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::
    set_is_subject_to_account_level_enterprise_policies(bool value) {
  capabilities_->capabilities_map_
      [kIsSubjectToAccountLevelEnterprisePoliciesCapabilityName] = value;
}

void AccountCapabilitiesTestMutator::
    set_is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(
        bool value) {
  capabilities_->capabilities_map_
      [kIsSubjectToChromePrivacySandboxRestrictedMeasurementNotice] = value;
}

void AccountCapabilitiesTestMutator::set_is_subject_to_enterprise_features(
    bool value) {
  capabilities_
      ->capabilities_map_[kIsSubjectToEnterprisePoliciesCapabilityName] = value;
}

void AccountCapabilitiesTestMutator::set_is_subject_to_parental_controls(
    bool value) {
  capabilities_->capabilities_map_[kIsSubjectToParentalControlsCapabilityName] =
      value;
}

// keep-sorted end

void AccountCapabilitiesTestMutator::SetAllSupportedCapabilities(bool value) {
  for (std::string_view name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    capabilities_->capabilities_map_[std::string(name)] = value;
  }
}

void AccountCapabilitiesTestMutator::SetCapability(const std::string& name,
                                                   bool value) {
  base::span<const std::string_view> capability_names =
      AccountCapabilities::GetSupportedAccountCapabilityNames();
  CHECK(base::Contains(capability_names, name))
      << "Invalid capability name: " << name;
  capabilities_->capabilities_map_[name] = value;
}
