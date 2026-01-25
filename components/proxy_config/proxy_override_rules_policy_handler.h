// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_OVERRIDE_RULES_POLICY_HANDLER_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_OVERRIDE_RULES_POLICY_HANDLER_H_

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/proxy_config/proxy_config_export.h"

namespace proxy_config {

class PROXY_CONFIG_EXPORT ProxyOverrideRulesPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ProxyOverrideRulesPolicyHandler(policy::Schema schema);
  ~ProxyOverrideRulesPolicyHandler() override;

  // policy::SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Helpers that parse the provided `value`s and checks that they are coherent
  // with the policy's expected formats. These functions are called after the
  // basic schema validation done in `CheckPolicySettings`. Returns false if the
  // passed `value` should result in its rule not being valid overall.
  bool CheckRule(const base::DictValue& value,
                 policy::PolicyErrorPath error_path,
                 policy::PolicyErrorMap* errors);
  bool CheckDestinations(const base::Value& value,
                         policy::PolicyErrorPath error_path,
                         policy::PolicyErrorMap* errors);
  bool CheckProxyList(const base::Value& value,
                      policy::PolicyErrorPath error_path,
                      policy::PolicyErrorMap* errors);
  bool CheckConditions(const base::Value& value,
                       policy::PolicyErrorPath error_path,
                       policy::PolicyErrorMap* errors);
  bool CheckConditionEntry(const base::Value& value,
                           policy::PolicyErrorPath error_path,
                           policy::PolicyErrorMap* errors);

  // Helper to populate errors in chrome://policy. No-op if `errors` is null.
  void AddError(int message_id,
                policy::PolicyErrorPath error_path,
                policy::PolicyErrorMap* errors);
  void AddError(int message_id,
                const std::string& parameter,
                policy::PolicyErrorPath error_path,
                policy::PolicyErrorMap* errors);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  policy::IntRangePolicyHandler enabled_for_all_users_handler_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
};

}  // namespace proxy_config
#endif  // COMPONENTS_PROXY_CONFIG_PROXY_OVERRIDE_RULES_POLICY_HANDLER_H_
