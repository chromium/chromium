// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_policy_utils.h"

#include <string>

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_net {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
bool IsUnaffiliatedUserProxyPolicyAllowed(const policy::PolicyMap& policies) {
  const policy::PolicyMap::Entry* entry =
      policies.Get(policy::key::kEnableProxyOverrideRulesForAllUsers);
  if (!entry || entry->scope != policy::POLICY_SCOPE_MACHINE) {
    return false;
  }
  const base::Value* value = entry->value(base::Value::Type::INTEGER);
  return value && value->GetInt() == 1;
}

bool CheckUserAffiliatedProxyPolicy(const policy::PolicyMap& policies,
                                    const std::string& policy_name,
                                    policy::PolicyErrorMap* errors) {
  if (errors &&
      !errors->HasError(policy::key::kEnableProxyOverrideRulesForAllUsers)) {
    policy::IntRangePolicyHandler enabled_for_all_users_handler(
        policy::key::kEnableProxyOverrideRulesForAllUsers,
        /*pref_path=*/nullptr,
        /*min=*/0,
        /*max=*/1,
        /*clamp=*/false);
    enabled_for_all_users_handler.CheckPolicySettings(policies, errors);
  }

  const policy::PolicyMap::Entry* entry = policies.Get(policy_name);
  if (!entry) {
    return true;
  }

  if (entry->scope == policy::PolicyScope::POLICY_SCOPE_USER &&
      !policies.GetDeviceAffiliationIds().empty() &&
      !policies.IsUserAffiliated() &&
      !IsUnaffiliatedUserProxyPolicyAllowed(policies)) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_UNAFFILIATED_USER_ERROR);
    }
    return false;
  }
  return true;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace enterprise_net
