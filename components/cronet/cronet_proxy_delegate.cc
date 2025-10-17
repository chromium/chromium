// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_proxy_delegate.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/proxy_resolution/proxy_info.h"
#include "url/gurl.h"

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
        auto chain =
            net::ProxyChain::WithOpaqueData(std::vector<net::ProxyServer>(),
                                            /*opaque_data=*/i);
        CHECK(chain.IsValid());
        proxy_list.AddProxyChain(std::move(chain));
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
        auto chain = net::ProxyChain::WithOpaqueData(
            std::vector<net::ProxyServer>{net::ProxyServer(
                scheme,
                net::HostPortPair(proxy_server.host(), proxy_server.port()))},
            /*opaque_data=*/i);
        CHECK(chain.IsValid());
        proxy_list.AddProxyChain(std::move(chain));
      }
    }
    result->UseProxyList(proxy_list);
  }();
  TRACE_EVENT_END("cronet", "resulting_proxy_info", result->ToDebugString());
}

std::optional<bool> CronetProxyDelegate::CanFalloverToNextProxyOverride(
    const net::ProxyChain& proxy_chain,
    int net_error) {
  // We promise this in org.chromium.net.ProxyOptions's documentation.
  return true;
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

base::expected<net::HttpRequestHeaders, net::Error>
CronetProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t proxy_index,
    OnBeforeTunnelRequestCallback callback) {
  TRACE_EVENT_BEGIN("cronet", "CronetProxyDelegate::OnBeforeTunnelRequest",
                    "proxy_chain", proxy_chain.ToDebugString(), "proxy_index",
                    proxy_index);
  CHECK(proxy_chain.opaque_data().has_value());
  // org.chromium.net.Proxy.Callback#onBeforeTunnelRequest always continues
  // asynchronously. So, from //net's perspective, this always ends up in
  // `net::ERR_IO_PENDING`.
  network_tasks_->OnBeforeTunnelRequest(*proxy_chain.opaque_data(),
                                        std::move(callback));
  const auto result = net::ERR_IO_PENDING;
  TRACE_EVENT_END("cronet", "result", result);
  return base::unexpected(result);
}

net::Error CronetProxyDelegate::OnTunnelHeadersReceived(
    const net::ProxyChain& proxy_chain,
    size_t proxy_index,
    const net::HttpResponseHeaders& response_headers,
    net::CompletionOnceCallback callback) {
  TRACE_EVENT_BEGIN("cronet", "CronetProxyDelegate::OnTunnelHeadersReceived",
                    "proxy_chain", proxy_chain.ToDebugString(), "proxy_index",
                    proxy_index);
  CHECK(proxy_chain.opaque_data().has_value());
  // org.chromium.net.Proxy.Callback#onTunnelHeadersReceived always continues
  // asynchronously. So, from //net's perspective, this always ends up in
  // `net::ERR_IO_PENDING`.
  network_tasks_->OnTunnelHeadersReceived(
      *proxy_chain.opaque_data(), response_headers, std::move(callback));
  const auto result = net::ERR_IO_PENDING;
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
