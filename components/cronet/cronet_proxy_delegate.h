// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_PROXY_DELEGATE_H_
#define COMPONENTS_CRONET_CRONET_PROXY_DELEGATE_H_

#include "base/types/expected.h"
#include "components/cronet/cronet_context.h"
#include "components/cronet/proto/request_context_config.pb.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"

class GURL;

namespace net {

class NetworkAnonymizationKey;
class ProxyChain;
class ProxyInfo;

}  // namespace net

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
  std::optional<bool> CanFalloverToNextProxyOverride(
      const net::ProxyChain& proxy_chain,
      int net_error) override;
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override;
  void OnSuccessfulRequestAfterFailures(
      const net::ProxyRetryInfoMap& proxy_retry_info) override;
  base::expected<net::HttpRequestHeaders, net::Error> OnBeforeTunnelRequest(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      OnBeforeTunnelRequestCallback callback) override;
  net::Error OnTunnelHeadersReceived(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      const net::HttpResponseHeaders& response_headers,
      net::CompletionOnceCallback callback) override;
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
