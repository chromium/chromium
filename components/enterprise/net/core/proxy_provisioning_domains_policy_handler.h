// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_PROXY_PROVISIONING_DOMAINS_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_PROXY_PROVISIONING_DOMAINS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace enterprise_net {

class ProxyProvisioningDomainsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ProxyProvisioningDomainsPolicyHandler(policy::Schema schema);
  ProxyProvisioningDomainsPolicyHandler(
      const ProxyProvisioningDomainsPolicyHandler&) = delete;
  ProxyProvisioningDomainsPolicyHandler& operator=(
      const ProxyProvisioningDomainsPolicyHandler&) = delete;
  ~ProxyProvisioningDomainsPolicyHandler() override;

  // policy::SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_PROXY_PROVISIONING_DOMAINS_POLICY_HANDLER_H_
