// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_PROXY_POLICY_UTILS_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_PROXY_POLICY_UTILS_H_

#include <string>

#include "build/build_config.h"

namespace policy {
class PolicyErrorMap;
class PolicyMap;
}  // namespace policy

namespace enterprise_net {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
// Returns true if the machine-scoped EnableProxyOverrideRulesForAllUsers policy
// is enabled (set to 1).
bool IsUnaffiliatedUserProxyPolicyAllowed(const policy::PolicyMap& policies);

// Validates that if the policy named `policy_name` is set at user scope on a
// managed browser, the user must be affiliated unless
// EnableProxyOverrideRulesForAllUsers is enabled.
// Also validates the value range of EnableProxyOverrideRulesForAllUsers.
// If validation fails and `errors` is non-null, adds the appropriate error
// to `errors`.
bool CheckUserAffiliatedProxyPolicy(const policy::PolicyMap& policies,
                                    const std::string& policy_name,
                                    policy::PolicyErrorMap* errors);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_PROXY_POLICY_UTILS_H_
