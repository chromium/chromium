// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_override_rules_policy_handler.h"

#include <variant>

#include "base/check.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs_utils.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/scheme_host_port_matcher_rule.h"

namespace proxy_config {

namespace {

policy::PolicyErrorPath CreateNewPath(
    policy::PolicyErrorPath path,
    std::variant<int, std::string> new_value) {
  path.push_back(std::move(new_value));
  return path;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
bool UnaffiliatedPolicyAllowed(const policy::PolicyMap& policies) {
  const base::Value* enable_for_all_users_value =
      policies.GetValue(policy::key::kEnableProxyOverrideRulesForAllUsers,
                        base::Value::Type::INTEGER);
  if (!enable_for_all_users_value) {
    return false;
  }

  return enable_for_all_users_value->GetInt() == 1;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

}  // namespace

ProxyOverrideRulesPolicyHandler::ProxyOverrideRulesPolicyHandler(
    policy::Schema schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kProxyOverrideRules,
          schema.GetKnownProperty(policy::key::kProxyOverrideRules),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      ,
      enabled_for_all_users_handler_(
          policy::key::kEnableProxyOverrideRulesForAllUsers,
          /*pref_path=*/nullptr,
          /*min=*/0,
          /*max=*/1,
          /*clamp*/ false)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
{
}

ProxyOverrideRulesPolicyHandler::~ProxyOverrideRulesPolicyHandler() = default;

bool ProxyOverrideRulesPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // This code should run to set errors for
  // `kEnableProxyOverrideRulesForAllUsers`.
  enabled_for_all_users_handler_.CheckPolicySettings(policies, errors);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  policy::SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  const policy::PolicyMap::Entry* proxy_override_rules_policy =
      policies.Get(policy_name());
  if (proxy_override_rules_policy &&
      proxy_override_rules_policy->scope ==
          policy::PolicyScope::POLICY_SCOPE_USER &&
      !policies.GetDeviceAffiliationIds().empty() &&
      !policies.IsUserAffiliated() && !UnaffiliatedPolicyAllowed(policies)) {
    errors->AddError(policy_name(), IDS_POLICY_UNAFFILIATED_USER_ERROR);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return true;
  }

  CHECK(value->is_list());
  const auto& rules_list = value->GetList();
  for (size_t i = 0; i < rules_list.size(); ++i) {
    CHECK(rules_list[i].is_dict());
    CheckRule(rules_list[i].GetDict(),
              CreateNewPath({}, base::checked_cast<int>(i)), errors);
  }

  // ALWAYS return true to ensure ApplyPolicySettings runs, which is needed to
  // update kProxyOverrideRulesAffiliation. The actual application of the
  // policy value is guarded by CheckAndGetValue in ApplyPolicySettings.
  return true;
}

void ProxyOverrideRulesPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // ALWAYS update affiliation, even if kProxyOverrideRules is not set.
  // This ensures the state is captured in the Managed pref store and
  // kept in sync with the latest policy bundle's affiliation status.
  prefs->SetBoolean(proxy_config::prefs::kProxyOverrideRulesAffiliation,
                    policies.GetDeviceAffiliationIds().empty() ||
                        policies.IsUserAffiliated());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  if (!policy) {
    return;
  }

  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, /*errors=*/nullptr, &policy_value) ||
      !policy_value || !policy_value->is_list()) {
    return;
  }

  policy_value->GetList().EraseIf([this](const base::Value& rule) {
    return !CheckRule(rule.GetDict(), /*error_path=*/{}, /*errors=*/nullptr);
  });

  prefs->SetValue(proxy_config::prefs::kProxyOverrideRules,
                  policy_value->Clone());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  prefs->SetInteger(proxy_config::prefs::kProxyOverrideRulesScope,
                    policy->scope);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}

bool ProxyOverrideRulesPolicyHandler::CheckRule(
    const base::DictValue& value,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  // Evaluate each sub-field of a given rule to get all error messages
  // populated instead of returning early.
  bool valid = true;

  // Mandatory fields.
  if (value.contains(kKeyDestinationMatchers)) {
    valid &= CheckDestinations(
        *value.Find(kKeyDestinationMatchers),
        CreateNewPath(error_path, kKeyDestinationMatchers), errors);
  } else {
    valid = false;
    AddError(IDS_POLICY_NOT_SPECIFIED_ERROR,
             CreateNewPath(error_path, kKeyDestinationMatchers), errors);
  }
  if (value.contains(kKeyProxyList)) {
    valid &= CheckProxyList(*value.Find(kKeyProxyList),
                            CreateNewPath(error_path, kKeyProxyList), errors);
  } else {
    valid = false;
    AddError(IDS_POLICY_NOT_SPECIFIED_ERROR,
             CreateNewPath(error_path, kKeyProxyList), errors);
  }

  // Optional fields.
  if (value.contains(kKeyExcludeDestinationMatchers)) {
    valid &= CheckDestinations(
        *value.Find(kKeyExcludeDestinationMatchers),
        CreateNewPath(error_path, kKeyExcludeDestinationMatchers), errors);
  }
  if (value.contains(kKeyConditions)) {
    valid &= CheckConditions(*value.Find(kKeyConditions),
                             CreateNewPath(error_path, kKeyConditions), errors);
  }

  return valid;
}

bool ProxyOverrideRulesPolicyHandler::CheckDestinations(
    const base::Value& value,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  CHECK(value.is_list());

  int i = 0;
  for (const auto& destination : value.GetList()) {
    CHECK(destination.is_string());
    if (!net::SchemeHostPortMatcherRule::FromUntrimmedRawString(
            destination.GetString())) {
      AddError(IDS_POLICY_PROXY_INVALID_DESTINATION, destination.GetString(),
               CreateNewPath(error_path, i), errors);
    }
    ++i;
  }

  // This function returns true even if invalid are found in the previous loop.
  // This is because only a single pattern in the destination list is needed to
  // match the "DestinationMatchers" or "ExcludedDestinationMatchers" fields, so
  // even if some are misconfigured the rule could still be triggered.
  return true;
}

bool ProxyOverrideRulesPolicyHandler::CheckProxyList(
    const base::Value& value,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  CHECK(value.is_list());

  bool valid = true;
  int i = 0;
  for (const auto& proxy : value.GetList()) {
    CHECK(proxy.is_string());
    net::ProxyChain chain =
        proxy_config::ProxyOverrideRuleProxyFromString(proxy.GetString());
    if (!chain.IsValid()) {
      valid = false;
      AddError(IDS_POLICY_PROXY_INVALID_PROXY, proxy.GetString(),
               CreateNewPath(error_path, i), errors);
    }
    ++i;
  }

  return valid;
}

bool ProxyOverrideRulesPolicyHandler::CheckConditions(
    const base::Value& value,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  CHECK(value.is_list());

  bool result = true;
  int i = 0;
  for (const auto& entry : value.GetList()) {
    // Since all conditions in a proxy override rule must be met for a rule to
    // match, an invalid condition means the whole rule should never trigger and
    // should therefore be ignored. The loop is not exited early to add warnings
    // to all conditions.
    if (!CheckConditionEntry(entry, CreateNewPath(error_path, i), errors)) {
      result = false;
    }

    ++i;
  }

  return result;
}

bool ProxyOverrideRulesPolicyHandler::CheckConditionEntry(
    const base::Value& value,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  CHECK(value.is_dict());

  // Conditions have the following format:
  // {
  //   "DnsProbe": {
  //     "Host": "corp.ads",
  //     "Result": "resolved", // or "not_found"
  //   }
  // }
  // Each field is mandatory.
  for (const auto condition : value.GetDict()) {
    if (condition.first != kKeyDnsProbe) {
      AddError(IDS_POLICY_PROXY_UNKNOWN_CONDITION, condition.first, error_path,
               errors);
      return false;
    }
  }

  const auto* dns_probe = value.GetDict().FindDict(kKeyDnsProbe);
  if (!dns_probe) {
    return false;
  }

  const auto* host = dns_probe->FindString(kKeyHost);
  const auto* result = dns_probe->FindString(kKeyResult);
  if (!host || !result) {
    if (!host) {
      AddError(IDS_POLICY_NOT_SPECIFIED_ERROR,
               CreateNewPath(error_path, kKeyHost), errors);
    }
    if (!result) {
      AddError(IDS_POLICY_NOT_SPECIFIED_ERROR,
               CreateNewPath(error_path, kKeyResult), errors);
    }
    return false;
  }

  auto scheme_host_port = proxy_config::ProxyOverrideRuleHostFromString(*host);
  if (!scheme_host_port.IsValid()) {
    AddError(IDS_POLICY_PROXY_INVALID_SCHEME_HOST_PORT, *host,
             CreateNewPath(error_path, kKeyHost), errors);
    return false;
  }

  return true;
}

void ProxyOverrideRulesPolicyHandler::AddError(
    int message_id,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  if (!errors) {
    return;
  }
  errors->AddError(policy_name(), message_id, error_path);
}

void ProxyOverrideRulesPolicyHandler::AddError(
    int message_id,
    const std::string& parameter,
    policy::PolicyErrorPath error_path,
    policy::PolicyErrorMap* errors) {
  if (!errors) {
    return;
  }
  errors->AddError(policy_name(), message_id, parameter, error_path);
}

}  // namespace proxy_config
