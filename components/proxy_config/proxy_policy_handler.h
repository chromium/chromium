// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_POLICY_HANDLER_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/proxy_config/proxy_config_export.h"

namespace proxy_config {

// ConfigurationPolicyHandler for the proxy policies.
class PROXY_CONFIG_EXPORT ProxyPolicyHandler
    : public policy::ConfigurationPolicyHandler {
 public:
  // Constants for the "Proxy Server Mode" defined in the policies.
  // Note that these diverge from internal presentation defined in
  // ProxyPrefs::ProxyMode for legacy reasons. The following four
  // PolicyProxyModeType types were not very precise and had overlapping use
  // cases.
  enum ProxyModeType {
    // Disable Proxy, connect directly.
    PROXY_SERVER_MODE = 0,
    // Auto detect proxy or use specific PAC script if given.
    PROXY_AUTO_DETECT_PROXY_SERVER_MODE = 1,
    // Use manually configured proxy servers (fixed servers).
    PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE = 2,
    // Use system proxy server.
    PROXY_USE_SYSTEM_PROXY_SERVER_MODE = 3,

    MODE_COUNT
  };

  ProxyPolicyHandler();
  ProxyPolicyHandler(const ProxyPolicyHandler&) = delete;
  ProxyPolicyHandler& operator=(const ProxyPolicyHandler&) = delete;
  ~ProxyPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace proxy_config

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_POLICY_HANDLER_H_
