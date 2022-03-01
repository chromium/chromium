// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_

#include <map>
#include <string>
#include <vector>

#include "components/signin/public/identity_manager/tribool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}

// Stores the information about account capabilities. Capabilities provide
// information about state and features of Gaia accounts.
class AccountCapabilities {
 public:
  AccountCapabilities();
  ~AccountCapabilities();
  AccountCapabilities(const AccountCapabilities& other);
  AccountCapabilities& operator=(const AccountCapabilities& other);

  // Chrome can offer extended promos for turning on Sync to accounts with this
  // capability.
  signin::Tribool can_offer_extended_chrome_sync_promos() const;

  // Chrome can run privacy sandbox trials for accounts with this capability.
  signin::Tribool can_run_chrome_privacy_sandbox_trials() const {
    // TODO(crbug.com/1298865): Replace with the actual capability when it is
    // available and update tests.
    return can_offer_extended_chrome_sync_promos();
  }

  // Whether none of the capabilities has `signin::Tribool::kUnknown`.
  bool AreAllCapabilitiesKnown() const;

  // Updates the capability state value for keys in `other`. If a value is
  // `signin::Tribool::kUnknown` in `other` the corresponding key will not
  // be updated in order to avoid overriding known values.
  bool UpdateWith(const AccountCapabilities& other);

 private:
  friend absl::optional<AccountCapabilities> AccountCapabilitiesFromValue(
      const base::Value& account_capabilities);
  friend class AccountCapabilitiesFetcher;
  friend class AccountCapabilitiesTestMutator;
  friend class AccountTrackerService;

  // Returns the capability state using the service name.
  signin::Tribool GetCapabilityByName(const std::string& name) const;

  // Returns the list of account capability service names supported in Chrome.
  static const std::vector<std::string>& GetSupportedAccountCapabilityNames();

  std::map<std::string, bool> capabilities_map_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_H_
