// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_

#include <vector>

#include "base/optional.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "net/proxy_resolution/proxy_list.h"

namespace net {
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyServer;

class DataReductionProxyConfigValues {
 public:
  virtual ~DataReductionProxyConfigValues() {}

  // Returns the HTTP proxy servers to be used. Proxies that cannot be used
  // because they are temporarily or permanently marked as bad are also
  // included.
  virtual const std::vector<DataReductionProxyServer>& proxies_for_http()
      const = 0;

  // Determines if the given |proxy_server| matches a currently or recent
  // previously configured Data Reduction Proxy server, returning information
  // about where that proxy is in the ordered list of proxies to use. It's up to
  // the implementation to determine what counts as a recent previously
  // configured Data Reduction Proxy server, but the idea is to be able to
  // recognize proxies from requests that use the currently configured
  // |proxies_for_http()| as well as recognize proxies from requests that are
  // in-progress when the list of proxy servers to use changes. If
  // |proxy_server| matches multiple proxies, then the most recent and highest
  // precedence result is returned.
  virtual base::Optional<DataReductionProxyTypeInfo>
  FindConfiguredDataReductionProxy(
      const net::ProxyServer& proxy_server) const = 0;

  // Gets all current and recently configured Data Reduction Proxy servers.
  virtual net::ProxyList GetAllConfiguredProxies() const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
