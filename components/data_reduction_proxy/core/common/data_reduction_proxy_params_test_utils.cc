// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"

namespace data_reduction_proxy {

TestDataReductionProxyParams::TestDataReductionProxyParams()
    : override_non_secure_proxies_(false) {
  proxies_for_http_.push_back(
      DataReductionProxyServer(net::ProxyServer::FromURI(
          "origin.net:80", net::ProxyServer::SCHEME_HTTP)));
  proxies_for_http_.push_back(
      DataReductionProxyServer(net::ProxyServer::FromURI(
          "fallback.net:80", net::ProxyServer::SCHEME_HTTP)));
  }

  TestDataReductionProxyParams::~TestDataReductionProxyParams() {}

  void TestDataReductionProxyParams::SetProxiesForHttp(
      const std::vector<DataReductionProxyServer>& proxies) {
    DCHECK_GE(2u, proxies_for_http_.size());

    size_t secure_proxies = 0;
    for (const auto& ps : proxies)
      if (ps.proxy_server().is_https())
        secure_proxies++;
    DCHECK_GE(1u, secure_proxies);

    SetProxiesForHttpForTesting(proxies);
    proxies_for_http_.clear();
    for (const auto& ps : proxies) {
      if (override_non_secure_proxies_ && ps.proxy_server().is_https()) {
        proxies_for_http_.push_back(
            DataReductionProxyServer(net::ProxyServer::FromURI(
                "origin.net:80", net::ProxyServer::SCHEME_HTTP)));
      } else {
        proxies_for_http_.push_back(ps);
      }
    }
  }

  const std::vector<DataReductionProxyServer>&
  TestDataReductionProxyParams::proxies_for_http() const {
    if (override_non_secure_proxies_)
      return proxies_for_http_;
    return DataReductionProxyParams::proxies_for_http();
}

net::ProxyList TestDataReductionProxyParams::GetAllConfiguredProxies() const {
  return configured_proxies_;
}

void TestDataReductionProxyParams::SetConfiguredProxies(
    const net::ProxyList& configured_proxies) {
  configured_proxies_ = configured_proxies;
}

}  // namespace data_reduction_proxy
