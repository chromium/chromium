// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_PROXY_DELEGATE_H_
#define COMPONENTS_CRONET_CRONET_PROXY_DELEGATE_H_

#include "components/cronet/cronet_context.h"
#include "components/cronet/proto/request_context_config.pb.h"
#include "net/base/proxy_delegate.h"

namespace cronet {

// Implements org.chromium.net.ProxyOptions by implementing a custom
// net::ProxyDelegate.
class CronetProxyDelegate final : public net::ProxyDelegate {
 public:
  // `proxy_config` represent the native view of org.chromium.net.ProxyOptions.
  // `network_tasks` is used to redirect callbacks to the `CronetContext` that
  // created this, it must outlive this class.
  CronetProxyDelegate(cronet::proto::ProxyOptions proxy_config,
                      CronetContext::NetworkTasks* network_tasks);
  ~CronetProxyDelegate() override;

  CronetProxyDelegate(const CronetProxyDelegate&) = delete;
  CronetProxyDelegate& operator=(const CronetProxyDelegate&) = delete;

  // net::ProxyDelegate implementation:
  void OnResolveProxy(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const std::string& method,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      net::ProxyInfo* result) override;
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override;
  void OnSuccessfulRequestAfterFailures(
      const net::ProxyRetryInfoMap& proxy_retry_info) override;
  net::Error OnBeforeTunnelRequest(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      net::HttpRequestHeaders* extra_headers) override;
  net::Error OnTunnelHeadersReceived(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      const net::HttpResponseHeaders& response_headers) override;
  void SetProxyResolutionService(
      net::ProxyResolutionService* proxy_resolution_service) override;
  bool AliasRequiresProxyOverride(
      const std::string scheme,
      const std::vector<std::string>& dns_aliases,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;

 private:
  const cronet::proto::ProxyOptions proxy_options_;
  const raw_ptr<CronetContext::NetworkTasks> network_tasks_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_PROXY_DELEGATE_H_
