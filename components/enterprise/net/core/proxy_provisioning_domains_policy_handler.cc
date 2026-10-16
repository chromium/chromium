// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_provisioning_domains_policy_handler.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/enterprise/net/core/proxy_policy_utils.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace enterprise_net {

ProxyProvisioningDomainsPolicyHandler::ProxyProvisioningDomainsPolicyHandler(
    policy::Schema schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kProxyProvisioningDomains,
          schema.GetKnownProperty(policy::key::kProxyProvisioningDomains),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

ProxyProvisioningDomainsPolicyHandler::
    ~ProxyProvisioningDomainsPolicyHandler() = default;

bool ProxyProvisioningDomainsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  if (!CheckUserAffiliatedProxyPolicy(policies, policy_name(), errors)) {
    return false;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

  return policy::SchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                    errors);
}

void ProxyProvisioningDomainsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> value;
  if (CheckAndGetValue(policies, nullptr, &value) && value) {
    prefs->SetValue(kProxyProvisioningDomains,
                    base::Value::FromUniquePtrValue(std::move(value)));
  }
}

}  // namespace enterprise_net
