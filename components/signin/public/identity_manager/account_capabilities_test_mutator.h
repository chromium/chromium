// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_TEST_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_TEST_MUTATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/account_capabilities.h"

// Support class that allows callers to modify internal capability state
// mappings used for tests.
class AccountCapabilitiesTestMutator {
 public:
  explicit AccountCapabilitiesTestMutator(AccountCapabilities* capabilities);

  // Exposes the full list of supported capabilities for tests.
  static const std::vector<std::string>& GetSupportedAccountCapabilityNames();

  // Exposes setters for the supported capabilities.
  // Please keep this list alphabetically sorted.
  void set_can_have_email_address_displayed(bool value);
  void set_can_offer_extended_chrome_sync_promos(bool value);
  void set_can_run_chrome_privacy_sandbox_trials(bool value);
  void set_is_opted_in_to_parental_supervision(bool value);
  void set_can_toggle_auto_updates(bool value);
  void set_is_allowed_for_machine_learning(bool value);
  void set_is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(
      bool value);
  void set_is_subject_to_enterprise_policies(bool value);
  void set_is_subject_to_parental_controls(bool value);

  // Modifies all supported capabilities at once.
  void SetAllSupportedCapabilities(bool value);

 private:
  raw_ptr<AccountCapabilities> capabilities_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_TEST_MUTATOR_H_
