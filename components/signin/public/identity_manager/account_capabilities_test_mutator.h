// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_TEST_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_TEST_MUTATOR_H_

#include "components/signin/public/identity_manager/account_capabilities.h"

// Support class that allows callers to modify internal capability state
// mappings used for tests.
class AccountCapabilitiesTestMutator {
 public:
  explicit AccountCapabilitiesTestMutator(AccountCapabilities* capabilities);

  // Exposes the full list of supported capabilities for tests.
  static const std::vector<std::string>& GetSupportedAccountCapabilityNames();

  // Exposes setters for the supported capabilities.
  void set_can_offer_extended_chrome_sync_promos(bool value);
  void set_can_run_chrome_privacy_sandbox_trials(bool value);

  // Modifies all supported capabilities at once.
  void SetAllSupportedCapabilities(bool value);

 private:
  AccountCapabilities* capabilities_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_TEST_MUTATOR_H_
