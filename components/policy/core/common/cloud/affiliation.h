// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_AFFILIATION_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_AFFILIATION_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "components/policy/policy_export.h"

namespace policy {

class CloudPolicyCore;

// Returns true if the user and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
POLICY_EXPORT bool IsAffiliated(const base::flat_set<std::string>& user_ids,
                                const base::flat_set<std::string>& device_ids);

// TODO(peletskyi): Remove email after affiliation based implementation will
// fully work. http://crbug.com/515476
// The function makes a decision if user with `user_affiliation_ids` and
// `email` is affiliated on the device with `device_affiliation_ids` and
// `enterprise_domain`.
POLICY_EXPORT bool IsUserAffiliated(
    const base::flat_set<std::string>& user_affiliation_ids,
    const base::flat_set<std::string>& device_affiliation_ids,
    std::string_view email);

// Get affiliations ids from the core's store if possible. Returns an empty set
// if not possible.
POLICY_EXPORT base::flat_set<std::string> GetAffiliationIdsFromCore(
    const policy::CloudPolicyCore& core,
    bool for_device);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_AFFILIATION_H_
