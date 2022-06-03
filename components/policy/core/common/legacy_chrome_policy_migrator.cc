// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/legacy_chrome_policy_migrator.h"

#include <string>

#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"

namespace policy {

LegacyChromePolicyMigrator::LegacyChromePolicyMigrator(const char* old_name,
                                                       const char* new_name)
    : migration_(old_name, new_name) {}

LegacyChromePolicyMigrator::LegacyChromePolicyMigrator(
    const char* old_name,
    const char* new_name,
    Migration::ValueTransform transform)
    : migration_(old_name, new_name, transform) {}

LegacyChromePolicyMigrator::~LegacyChromePolicyMigrator() = default;

void LegacyChromePolicyMigrator::Migrate(policy::PolicyBundle* bundle) {
  policy::PolicyMap& chrome_map =
      bundle->Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, ""));

  CopyPolicyIfUnset(chrome_map, &chrome_map, migration_);
}

}  // namespace policy
