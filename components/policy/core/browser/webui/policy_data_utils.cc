// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_data_utils.h"

#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

std::optional<std::string> GetManagedBy(
    const policy::CloudPolicyManager* manager) {
  if (!manager) {
    return std::nullopt;
  }

  const policy::CloudPolicyStore* store = manager->core()->store();
  if (!store) {
    return std::nullopt;
  }

  const enterprise_management::PolicyData* policy = store->policy();
  if (!policy || !policy->has_managed_by()) {
    return std::nullopt;
  }

  return policy->managed_by();
}

}  // namespace policy
