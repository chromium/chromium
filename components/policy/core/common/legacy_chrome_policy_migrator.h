// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_LEGACY_CHROME_POLICY_MIGRATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_LEGACY_CHROME_POLICY_MIGRATOR_H_

#include "components/policy/core/common/policy_migrator.h"

namespace policy {

// LegacyChromePolicyMigrator migrates a deprecated Chrome domain policy to a
// new name, setting up the new policy based on the old one.
//
// This is intended to be used for policies that do not have a corresponding
// pref. If the policy has a pref, please use
// |LegacyPoliciesDeprecatingPolicyHandler| instead.
class POLICY_EXPORT LegacyChromePolicyMigrator : public PolicyMigrator {
 public:
  using Migration = PolicyMigrator::Migration;

  LegacyChromePolicyMigrator(const char* old_name, const char* new_name);
  LegacyChromePolicyMigrator(const char* old_name,
                             const char* new_name,
                             Migration::ValueTransform transform);
  ~LegacyChromePolicyMigrator() override;

  LegacyChromePolicyMigrator(const LegacyChromePolicyMigrator&) = delete;
  LegacyChromePolicyMigrator& operator=(const LegacyChromePolicyMigrator&) =
      delete;

  void Migrate(policy::PolicyBundle* bundle) override;

 private:
  Migration migration_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_LEGACY_CHROME_POLICY_MIGRATOR_H_
