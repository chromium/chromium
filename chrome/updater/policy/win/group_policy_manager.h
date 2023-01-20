// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_

#include <string>

#include "chrome/updater/policy/policy_manager.h"

namespace updater {

// The GroupPolicyManager returns policies for domain-joined machines.
class GroupPolicyManager : public PolicyManager {
 public:
  explicit GroupPolicyManager(bool should_take_policy_critical_section);
  GroupPolicyManager(const GroupPolicyManager&) = delete;
  GroupPolicyManager& operator=(const GroupPolicyManager&) = delete;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;
  bool HasActiveDevicePolicies() const override;

 private:
  ~GroupPolicyManager() override;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_WIN_GROUP_POLICY_MANAGER_H_
