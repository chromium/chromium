// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_FETCHER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"

namespace net {

class ProxyChain;

}  // namespace net

namespace ip_protection {

struct GeoHint;

// Manages fetching the proxy configuration.
//
// This is an interface which is implemented differently on different platforms.
class IpProtectionProxyConfigFetcher {
 public:
  using GetProxyConfigCallback = base::OnceCallback<void(
      const std::optional<std::vector<::net::ProxyChain>>,
      const std::optional<GeoHint>)>;

  virtual ~IpProtectionProxyConfigFetcher();

  // Get proxy configuration that is necessary for IP Protection from the
  // server. If it is too soon to fetch the config, or fetching fails for any
  // reason, the callback will be invoked with `nulllopt` for all arguments.
  virtual void GetProxyConfig(GetProxyConfigCallback callback) = 0;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_FETCHER_H_
