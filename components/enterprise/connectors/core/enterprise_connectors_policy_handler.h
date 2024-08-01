// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_CONNECTORS_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_CONNECTORS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace enterprise_connectors {

// A schema policy handler for Enterprise Connectors policies that only accept
// cloud sources.
class EnterpriseConnectorsPolicyHandler
    : public policy::CloudOnlyPolicyHandler {
 public:
  EnterpriseConnectorsPolicyHandler(const char* policy_name,
                                    const char* pref_path,
                                    policy::Schema schema);
  EnterpriseConnectorsPolicyHandler(const char* policy_name,
                                    const char* pref_path,
                                    const char* pref_scope_path,
                                    policy::Schema schema);
  EnterpriseConnectorsPolicyHandler(EnterpriseConnectorsPolicyHandler&) =
      delete;
  EnterpriseConnectorsPolicyHandler& operator=(
      EnterpriseConnectorsPolicyHandler&) = delete;
  ~EnterpriseConnectorsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* pref_path_;

  // Key used to store the policy::PolicyScope of the policy. This is looked up
  // later so the Connector can adjust its behaviour.  May be null.
  const char* pref_scope_path_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_CONNECTORS_POLICY_HANDLER_H_
