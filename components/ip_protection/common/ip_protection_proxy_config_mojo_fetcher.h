// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_MOJO_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_MOJO_FETCHER_H_

#include "base/memory/scoped_refptr.h"
#include "components/ip_protection/common/ip_protection_proxy_config_fetcher.h"

namespace ip_protection {

class IpProtectionCoreHostRemote;

// Manages fetching the proxy configuration via Mojo. This is a simple wrapper
// around a config getter, which wraps the `Remote<CoreHost>`.
class IpProtectionProxyConfigMojoFetcher
    : public IpProtectionProxyConfigFetcher {
 public:
  explicit IpProtectionProxyConfigMojoFetcher(
      scoped_refptr<IpProtectionCoreHostRemote> core_host);
  ~IpProtectionProxyConfigMojoFetcher() override;

  // IpProtectionProxyConfigFetcher implementation.
  void GetProxyConfig(GetProxyConfigCallback callback) override;

 private:
  scoped_refptr<IpProtectionCoreHostRemote> core_host_remote_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_MOJO_FETCHER_H_
