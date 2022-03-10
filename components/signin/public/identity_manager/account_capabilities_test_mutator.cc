// Copyright 2022 The Chromium Authors. All rights reserved.
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

void AccountCapabilitiesTestMutator::SetAllSupportedCapabilities(bool value) {
  for (const std::string& name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    capabilities_->capabilities_map_[name] = value;
  }
}
