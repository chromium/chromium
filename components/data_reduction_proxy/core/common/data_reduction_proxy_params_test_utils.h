// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_

#include <vector>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace data_reduction_proxy {

class DataReductionProxyServer;

class TestDataReductionProxyParams : public DataReductionProxyParams {
 public:
  TestDataReductionProxyParams();

  ~TestDataReductionProxyParams() override;

  void SetProxiesForHttp(const std::vector<DataReductionProxyServer>& proxies);

  // Use non-secure data saver proxies. Useful when a URL request is fetched
  // from non-SSL mock sockets.
  void UseNonSecureProxiesForHttp() { override_non_secure_proxies_ = true; }

  const std::vector<DataReductionProxyServer>& proxies_for_http()
      const override;

  net::ProxyList GetAllConfiguredProxies() const override;

  void SetConfiguredProxies(const net::ProxyList& configured_proxies);

 private:
  bool override_non_secure_proxies_;

  std::vector<DataReductionProxyServer> proxies_for_http_;
  net::ProxyList configured_proxies_;
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
