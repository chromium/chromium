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

void AccountCapabilitiesTestMutator::set_can_have_email_address_displayed(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanHaveEmailAddressDisplayedCapabilityName] = value;
}

void AccountCapabilitiesTestMutator::set_can_offer_extended_chrome_sync_promos(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanOfferExtendedChromeSyncPromosCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_run_chrome_privacy_sandbox_trials(
    bool value) {
  capabilities_
      ->capabilities_map_[kCanRunChromePrivacySandboxTrialsCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_stop_parental_supervision(
    bool value) {
  capabilities_->capabilities_map_[kCanStopParentalSupervisionCapabilityName] =
      value;
}

void AccountCapabilitiesTestMutator::set_can_toggle_auto_updates(bool value) {
  capabilities_->capabilities_map_[kCanToggleAutoUpdatesName] = value;
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

void AccountCapabilitiesTestMutator::SetAllSupportedCapabilities(bool value) {
  for (const std::string& name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    capabilities_->capabilities_map_[name] = value;
  }
}
