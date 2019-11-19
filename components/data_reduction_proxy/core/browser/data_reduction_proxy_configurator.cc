// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/proxy_resolution/proxy_config.h"

namespace data_reduction_proxy {

DataReductionProxyConfigurator::DataReductionProxyConfigurator() {
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigurator::~DataReductionProxyConfigurator() {
}

void DataReductionProxyConfigurator::Enable(
    const NetworkPropertiesManager& network_properties_manager,
    const std::vector<DataReductionProxyServer>& proxies_for_http) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!params::IsIncludedInHoldbackFieldTrial() || proxies_for_http.empty());

  net::ProxyConfig config =
      CreateProxyConfig(false /* probe_url_config */,
                        network_properties_manager, proxies_for_http);
  config_ = config;
  if (config_updated_callback_)
    config_updated_callback_.Run();
}

net::ProxyConfig DataReductionProxyConfigurator::CreateProxyConfig(
    bool probe_url_config,
    const NetworkPropertiesManager& network_properties_manager,
    const std::vector<DataReductionProxyServer>& proxies_for_http) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!params::IsIncludedInHoldbackFieldTrial() || proxies_for_http.empty());

  net::ProxyConfig config;
  DCHECK(config.proxy_rules().proxies_for_http.IsEmpty());
  config.proxy_rules().type =
      net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;

  for (const auto& http_proxy : proxies_for_http) {
    // If the config is being generated for fetching the probe URL, then the
    // proxies that are disabled by the network properties manager are still
    // added to the proxy config. This is because the network properties manager
    // may have disabled a proxy because the warmup URL to the proxy has failed
    // in this session or in a previous session on the same connection. Adding
    // the proxy enables probing the proxy even though the proxy may not be
    // usable for non-proble traffic.
    if (!probe_url_config &&
        !network_properties_manager.IsSecureProxyAllowed(true) &&
        http_proxy.IsSecureProxy()) {
      continue;
    }

    if (!probe_url_config &&
        !network_properties_manager.IsInsecureProxyAllowed(true) &&
        !http_proxy.IsSecureProxy()) {
      continue;
    }

    config.proxy_rules().proxies_for_http.AddProxyServer(
        http_proxy.proxy_server());
  }

  if (!config.proxy_rules().proxies_for_http.IsEmpty()) {
    config.proxy_rules().proxies_for_http.AddProxyServer(
        net::ProxyServer::Direct());
  }

  if (config.proxy_rules().proxies_for_http.IsEmpty()) {
    // Return a DIRECT net config so that data reduction proxy is not used.
    return net::ProxyConfig::CreateDirect();
  }

  config.proxy_rules().bypass_rules = bypass_rules_;
  return config;
}

void DataReductionProxyConfigurator::Disable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyConfig config = net::ProxyConfig::CreateDirect();
  config_ = config;
  if (config_updated_callback_)
    config_updated_callback_.Run();
}

void DataReductionProxyConfigurator::SetBypassRules(
    const std::string& pattern) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bypass_rules_.ParseFromString(pattern);
}

const net::ProxyConfig& DataReductionProxyConfigurator::GetProxyConfig() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

}  // namespace data_reduction_proxy
