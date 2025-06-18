// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_proxy_delegate.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"

namespace cronet {

CronetProxyDelegate::CronetProxyDelegate(
    cronet::proto::ProxyOptions proxy_options,
    CronetContext::NetworkTasks* network_tasks)
    : proxy_options_(std::move(proxy_options)), network_tasks_(network_tasks) {
  CHECK(network_tasks);
}

CronetProxyDelegate::~CronetProxyDelegate() = default;

void CronetProxyDelegate::OnResolveProxy(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  TRACE_EVENT_BEGIN("cronet", "CronetProxyDelegate::OnResolveProxy", "url", url,
                    "method", method, "initial_proxy_info",
                    result->ToDebugString());
  // Using a self-calling lambda to make sure we always call TRACE_EVENT_END
  // regardless of early returns.
  [&] {
    net::ProxyList proxy_list;
    for (int i = 0; i < proxy_options_.proxies_size(); ++i) {
      const auto& proxy_server = proxy_options_.proxies(i);
      if (proxy_server.scheme() == cronet::proto::ProxyScheme::DIRECT) {
        proxy_list.AddProxyChain(
            net::ProxyChain::WithOpaqueData(std::vector<net::ProxyServer>(),
                                            /*opaque_data=*/i));
      } else {
        net::ProxyServer::Scheme scheme =
            net::ProxyServer::Scheme::SCHEME_INVALID;
        switch (proxy_server.scheme()) {
          case cronet::proto::ProxyScheme::HTTP:
            scheme = net::ProxyServer::Scheme::SCHEME_HTTP;
            break;
          case cronet::proto::ProxyScheme::HTTPS:
            scheme = net::ProxyServer::Scheme::SCHEME_HTTPS;
            break;
          default:
            NOTREACHED();
        }
        proxy_list.AddProxyChain(net::ProxyChain::WithOpaqueData(
            std::vector<net::ProxyServer>{net::ProxyServer(
                scheme,
                net::HostPortPair(proxy_server.host(), proxy_server.port()))},
            /*opaque_data=*/i));
      }
    }
    result->UseProxyList(proxy_list);
  }();
  TRACE_EVENT_END("cronet", "resulting_proxy_info", result->ToDebugString());
}

void CronetProxyDelegate::OnSuccessfulRequestAfterFailures(
    const net::ProxyRetryInfoMap& proxy_retry_info) {
  TRACE_EVENT_INSTANT("cronet",
                      "CronetProxyDelegate::OnSuccessfulRequestAfterFailures");
}

void CronetProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                     int net_error) {
  TRACE_EVENT_INSTANT("cronet", "CronetProxyDelegate::OnFallback", "bad_chain",
                      bad_chain.ToDebugString(), "net_error", net_error);
}

net::Error CronetProxyDelegate::OnBeforeTunnelRequest(
    // Don't be confused, this is the index of the proxy within the chain, not
    // the index of the chain itself.
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    net::HttpRequestHeaders* extra_headers) {
  TRACE_EVENT_BEGIN("cronet", "CronetProxyDelegate::OnBeforeTunnelRequest",
                    "proxy_chain", proxy_chain.ToDebugString(), "chain_index",
                    chain_index);
  CHECK(proxy_chain.opaque_data().has_value());
  auto result = [&] {
    if (network_tasks_->OnBeforeTunnelRequest(*proxy_chain.opaque_data(),
                                              extra_headers)) {
      return net::OK;
    }
    // TODO(https://crbug.com/422428959): Decide whether we want to propagate
    // org.chromium.net.Proxy.Callback canceling a tunnel establishment request
    // as net::ERR_TUNNEL_CONNECTION_FAILED. This is currently not possible, as
    // net::ProxyFallback::CanFalloverToNextProxy does not try the next proxy in
    // the list for net::ERR_TUNNEL_CONNECTION_FAILED, unless the chain is for
    // IP Protection. For the time being, we return another error for which the
    // next proxy is in the list is always attempted.
    return net::ERR_CONNECTION_CLOSED;
  }();
  TRACE_EVENT_END("cronet", "result", result);
  return result;
}

net::Error CronetProxyDelegate::OnTunnelHeadersReceived(
    // Don't be confused, this is the index of the proxy within the chain, not
    // the index of the chain itself.
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    const net::HttpResponseHeaders& response_headers) {
  TRACE_EVENT_BEGIN("cronet", "CronetProxyDelegate::OnTunnelHeadersReceived",
                    "proxy_chain", proxy_chain.ToDebugString(), "chain_index",
                    chain_index);
  CHECK(proxy_chain.opaque_data().has_value());
  auto result = [&] {
    if (network_tasks_->OnTunnelHeadersReceived(*proxy_chain.opaque_data(),
                                                response_headers)) {
      return net::OK;
    }
    // TODO(https://crbug.com/422428959): Decide whether we want to propagate
    // org.chromium.net.Proxy.Callback canceling a tunnel establishment request
    // as net::ERR_TUNNEL_CONNECTION_FAILED. This is currently not possible, as
    // net::ProxyFallback::CanFalloverToNextProxy does not try the next proxy in
    // the list for net::ERR_TUNNEL_CONNECTION_FAILED, unless the chain is for
    // IP Protection. For the time being, we return another error for which the
    // next proxy is in the list is always attempted.
    return net::ERR_CONNECTION_CLOSED;
  }();
  TRACE_EVENT_END("cronet", "result", result);
  return result;
}

void CronetProxyDelegate::SetProxyResolutionService(
    net::ProxyResolutionService* proxy_resolution_service) {
  TRACE_EVENT_INSTANT("cronet",
                      "CronetProxyDelegate::SetProxyResolutionService");
}

bool CronetProxyDelegate::AliasRequiresProxyOverride(
    const std::string scheme,
    const std::vector<std::string>& dns_aliases,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  TRACE_EVENT_INSTANT("cronet",
                      "CronetProxyDelegate::AliasRequiresProxyOverride");
  return false;
}

}  // namespace cronet
