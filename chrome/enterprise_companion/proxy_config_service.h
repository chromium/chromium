// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_PROXY_CONFIG_SERVICE_H_
#define CHROME_ENTERPRISE_COMPANION_PROXY_CONFIG_SERVICE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/proxy_resolution/proxy_config.h"

namespace device_management_storage {
class DMStorage;
}

namespace net {
class ProxyConfigService;
}

namespace enterprise_companion {

struct ProxyConfigAndOverridePrecedence {
  net::ProxyConfig config;
  std::optional<bool> cloud_policy_overrides_platform_policy;
};

// Creates a ProxyConfigService which sources proxy configurations from cloud
// policies (via `dm_storage_provider`) and local system policies (via
// `system_policy_proxy_config_provider`). If no policies are present, the
// fallback service is used.
std::unique_ptr<net::ProxyConfigService> CreatePolicyProxyConfigService(
    scoped_refptr<device_management_storage::DMStorage> dm_storage,
    base::RepeatingCallback<std::optional<ProxyConfigAndOverridePrecedence>()>
        system_policy_proxy_config_provider,
    std::unique_ptr<net::ProxyConfigService> fallback);

// Returns a callback which retrieves the proxy config settings from local
// system policies (i.e. Group Policy on Windows).
base::RepeatingCallback<std::optional<ProxyConfigAndOverridePrecedence>()>
GetDefaultSystemPolicyProxyConfigProvider();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_PROXY_CONFIG_SERVICE_H_
