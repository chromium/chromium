// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"

namespace data_reduction_proxy {

class NetworkPropertiesManager;

class DataReductionProxyConfigurator {
 public:
  DataReductionProxyConfigurator();

  ~DataReductionProxyConfigurator();

  // Enables data reduction using the proxy servers in |proxies_for_http|.
  void Enable(const NetworkPropertiesManager& network_properties_manager,
              const std::vector<DataReductionProxyServer>& proxies_for_http);

  // Constructs a proxy configuration suitable for disabling the Data Reduction
  // proxy.
  void Disable();

  // Sets the host patterns to bypass.
  //
  // See net::ProxyBypassRules::ParseFromString for the appropriate syntax.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  void SetBypassRules(const std::string& patterns);

  // Returns the current data reduction proxy config, even if it is not the
  // effective configuration used by the proxy service.
  const net::ProxyConfig& GetProxyConfig() const;

  // Constructs a proxy configuration suitable for enabling the Data Reduction
  // proxy. |probe_url_config| should be true if the proxy config is needed for
  // fetching the probe URL. If |probe_url_config| is true, then proxies that
  // are temporarily disabled may be included in the generated proxy config.
  net::ProxyConfig CreateProxyConfig(
      bool probe_url_config,
      const NetworkPropertiesManager& network_properties_manager,
      const std::vector<DataReductionProxyServer>& proxies_for_http) const;

  void SetConfigUpdatedCallback(
      base::RepeatingClosure config_updated_callback) {
    config_updated_callback_ = config_updated_callback;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfiguratorTest, TestBypassList);

  // Rules for bypassing the Data Reduction Proxy.
  net::ProxyBypassRules bypass_rules_;

  // The Data Reduction Proxy's configuration. This contains the list of
  // acceptable data reduction proxies and bypass rules, or DIRECT if DRP is not
  // enabled. It should be accessed only on the IO thread.
  net::ProxyConfig config_;

  base::RepeatingClosure config_updated_callback_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigurator);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
