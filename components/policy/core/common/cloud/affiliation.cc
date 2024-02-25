// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/affiliation.h"

#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

bool IsAffiliated(const base::flat_set<std::string>& user_ids,
                  const base::flat_set<std::string>& device_ids) {
  for (const std::string& device_id : device_ids) {
    if (user_ids.count(device_id))
      return true;
  }
  return false;
}

bool IsUserAffiliated(const base::flat_set<std::string>& user_affiliation_ids,
                      const base::flat_set<std::string>& device_affiliation_ids,
                      std::string_view email) {
  // An empty username means incognito user in case of Chrome OS and no
  // logged-in user in case of Chrome (SigninService). Many tests use nonsense
  // email addresses (e.g. 'test') so treat those as non-enterprise users.
  if (email.empty() || email.find('@') == std::string_view::npos) {
    return false;
  }

  if (IsDeviceLocalAccountUser(email)) {
    return true;
  }

  return IsAffiliated(user_affiliation_ids, device_affiliation_ids);
}

base::flat_set<std::string> GetAffiliationIdsFromCore(
    const policy::CloudPolicyCore& core,
    bool for_device) {
  // Validate client.
  if (!(core.client() && core.client()->is_registered())) {
    // Returns an empty set if the client isn't registered.
    return {};
  }

  // Check that a core with a registered client MUST have a store instance.
  CHECK(core.store());

  // Validate store.
  if (!core.store()->has_policy()) {
    // Returns an empty set if there is no policy data in the store.
    return {};
  }

  const auto* policy_data = core.store()->policy();
  const auto ids = for_device ? policy_data->device_affiliation_ids()
                              : policy_data->user_affiliation_ids();

  return {ids.begin(), ids.end()};
}

}  // namespace policy
