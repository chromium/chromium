// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_MIGRATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_MIGRATOR_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

namespace policy {

// A helper class that migrates a deprecated policy to a new policy -
// potentially across domain boundaries, by setting up the new policy based on
// the old one. It can migrate a deprecated policy to a new policy.
//
// For migrations that are only in the Chrome domain and which are accessed via
// prefs: you should use |LegacyPoliciesDeprecatingPolicyHandler| instead.
class POLICY_EXPORT PolicyMigrator {
 public:
  virtual ~PolicyMigrator();

  // If there are deprecated policies in |bundle|, set the value of the new
  // policies accordingly.
  virtual void Migrate(PolicyBundle* bundle) = 0;

  // Indicates how to rename a policy when migrating the old policy to the new
  // policy.
  struct POLICY_EXPORT Migration {
    using ValueTransform = base::RepeatingCallback<void(base::Value*)>;

    Migration(Migration&&);
    Migration(const char* old_name, const char* new_name);
    Migration(const char* old_name,
              const char* new_name,
              ValueTransform transform);
    ~Migration();

    // Old name for the policy
    const char* old_name;
    // New name for the policy, in the Chrome domain.
    const char* new_name;
    // Function to use to convert values from the old policy to the new
    // policy (e.g. convert value types). It should mutate the Value in
    // place. By default, it does no transform.
    ValueTransform transform;
  };

 protected:
  static void CopyPolicyIfUnset(PolicyMap& source,
                                PolicyMap* dest,
                                const Migration& migration);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_MIGRATOR_H_
