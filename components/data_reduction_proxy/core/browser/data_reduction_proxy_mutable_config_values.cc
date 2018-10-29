// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include <algorithm>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "net/base/proxy_server.h"

namespace data_reduction_proxy {

DataReductionProxyMutableConfigValues::DataReductionProxyMutableConfigValues()
    : use_override_proxies_for_http_(
          params::GetOverrideProxiesForHttpFromCommandLine(
              &override_proxies_for_http_)) {
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyMutableConfigValues::
    ~DataReductionProxyMutableConfigValues() {
}

const std::vector<DataReductionProxyServer>&
DataReductionProxyMutableConfigValues::proxies_for_http() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_override_proxies_for_http_ && !proxies_for_http_.empty()) {
    // Only override the proxies if a non-empty list of proxies would have been
    // returned otherwise. This is to prevent use of the proxies when the config
    // has been invalidated, since attempting to use a Data Reduction Proxy
    // without valid credentials could cause a proxy bypass.
    return override_proxies_for_http_;
  }
  return proxies_for_http_;
}

base::Optional<DataReductionProxyTypeInfo>
DataReductionProxyMutableConfigValues::FindConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Optional<DataReductionProxyTypeInfo> info =
      params::FindConfiguredProxyInVector(proxies_for_http(), proxy_server);
  if (info)
    return info;

  for (const auto& recent_proxies : recently_configured_proxy_lists_) {
    base::Optional<DataReductionProxyTypeInfo> recent_info =
        params::FindConfiguredProxyInVector(recent_proxies, proxy_server);
    if (recent_info)
      return recent_info;
  }
  return base::nullopt;
}

net::ProxyList DataReductionProxyMutableConfigValues::GetAllConfiguredProxies()
    const {
  net::ProxyList proxies;
  for (const auto& proxy : proxies_for_http())
    proxies.AddProxyServer(proxy.proxy_server());

  for (const auto& recent_proxies : recently_configured_proxy_lists_) {
    for (const auto& proxy : recent_proxies)
      proxies.AddProxyServer(proxy.proxy_server());
  }

  return proxies;
}

void DataReductionProxyMutableConfigValues::UpdateValues(
    const std::vector<DataReductionProxyServer>& new_proxies_for_http) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<DataReductionProxyServer> previous_proxies = proxies_for_http();

  proxies_for_http_.clear();
  std::remove_copy_if(new_proxies_for_http.begin(), new_proxies_for_http.end(),
                      std::back_inserter(proxies_for_http_),
                      [](const DataReductionProxyServer& proxy) {
                        return !proxy.proxy_server().is_valid() ||
                               proxy.proxy_server().is_direct();
                      });

  if (previous_proxies.empty() || proxies_for_http() == previous_proxies) {
    // There's no point in keeping track of an empty recent list of proxies or a
    // list of proxies that's identical to the currently configured list.
    return;
  }

  // Push |previous_proxies| onto the front of the
  // |recently_configured_proxy_lists_|.
  std::move_backward(std::begin(recently_configured_proxy_lists_),
                     std::end(recently_configured_proxy_lists_) - 1,
                     std::end(recently_configured_proxy_lists_));
  recently_configured_proxy_lists_[0] = std::move(previous_proxies);
}

void DataReductionProxyMutableConfigValues::Invalidate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateValues(std::vector<DataReductionProxyServer>());
}

}  // namespace data_reduction_proxy
