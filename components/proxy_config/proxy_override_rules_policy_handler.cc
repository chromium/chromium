// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_override_rules_policy_handler.h"

#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/proxy_config/proxy_config_pref_names.h"

namespace proxy_config {

ProxyOverrideRulesPolicyHandler::ProxyOverrideRulesPolicyHandler(
    policy::Schema schema)
    : policy::CloudOnlyPolicyHandler(
          policy::key::kProxyOverrideRules,
          schema.GetKnownProperty(policy::key::kProxyOverrideRules),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

ProxyOverrideRulesPolicyHandler::~ProxyOverrideRulesPolicyHandler() = default;

void ProxyOverrideRulesPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  if (!policy) {
    return;
  }

  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, /*errors=*/nullptr, &policy_value) ||
      !policy_value || !policy_value->is_list()) {
    return;
  }

  prefs->SetValue(proxy_config::prefs::kProxyOverrideRules,
                  policy_value->Clone());
#if !BUILDFLAG(IS_CHROMEOS)
  prefs->SetInteger(proxy_config::prefs::kProxyOverrideRulesScope,
                    policy->scope);
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace proxy_config
