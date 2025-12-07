// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_OVERRIDE_RULES_POLICY_HANDLER_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_OVERRIDE_RULES_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/proxy_config/proxy_config_export.h"

namespace proxy_config {

class PROXY_CONFIG_EXPORT ProxyOverrideRulesPolicyHandler
    : public policy::CloudOnlyPolicyHandler {
 public:
  explicit ProxyOverrideRulesPolicyHandler(policy::Schema schema);
  ~ProxyOverrideRulesPolicyHandler() override;

  // policy::CloudOnlyPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
};

}  // namespace proxy_config
#endif  // COMPONENTS_PROXY_CONFIG_PROXY_OVERRIDE_RULES_POLICY_HANDLER_H_
