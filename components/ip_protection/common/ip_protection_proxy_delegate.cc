// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_delegate.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager_impl.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/url_constants.h"

namespace ip_protection {

IpProtectionProxyDelegate::IpProtectionProxyDelegate(
    MaskedDomainListManager* masked_domain_list_manager,
    std::unique_ptr<IpProtectionCore> ipp_core)
    : masked_domain_list_manager_(masked_domain_list_manager),
      ipp_core_(std::move(ipp_core)) {
  CHECK(masked_domain_list_manager_);
  CHECK(masked_domain_list_manager_->IsEnabled());
  CHECK(ipp_core_);
}

IpProtectionProxyDelegate::~IpProtectionProxyDelegate() = default;

ProxyResolutionResult IpProtectionProxyDelegate::ClassifyRequest(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::ProxyInfo* result) {
  auto vlog = [&](std::string message) {
    std::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    VLOG(3) << "IPPD::ClassifyRequest(" << url << ", "
            << (top_frame_site.has_value() ? top_frame_site.value()
                                           : net::SchemefulSite())
            << ") - " << message;
  };

  const std::string& always_proxy = net::features::kIpPrivacyAlwaysProxy.Get();
  if (!always_proxy.empty()) {
    if (url.host() == always_proxy) {
      return ProxyResolutionResult::kAttemptProxy;
    }
    return ProxyResolutionResult::kNoMdlMatch;
  }

  // Check eligibility of this request.
  if (!masked_domain_list_manager_->IsPopulated()) {
    vlog("proxy allow list not populated");
    return ProxyResolutionResult::kMdlNotPopulated;
  } else if (!masked_domain_list_manager_->Matches(url,
                                                   network_anonymization_key)) {
    vlog("proxy allow list did not match");
    return ProxyResolutionResult::kNoMdlMatch;
  } else {
    vlog("proxy allow list matched");
  }

  result->set_is_mdl_match(true);

  // Check availability. We do not proxy requests if:
  // - The allow list has not been populated.
  // - The request doesn't match the allow list.
  // - The token cache is not available.
  // - The token cache does not have tokens.
  // - No proxy list is available.
  // - `kEnableIpProtection` is `false`.
  // - `is_ip_protection_enabled_` is `false` (in other words, the user has
  //   disabled IP Protection via user settings).
  // - `kIpPrivacyDirectOnly` is `true`.

  // TODO(https://crbug.com/40947771): Once the WebView traffic experiment is
  // done and IpProtectionProxyDelegate is only created in cases where IP
  // Protection should be used, remove this check.
  if (!base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy)) {
    vlog("ip protection proxy cannot be enabled");
    return ProxyResolutionResult::kFeatureDisabled;
  }

  if (!ipp_core_->IsIpProtectionEnabled()) {
    vlog("ip protection proxy is not currently enabled");
    return ProxyResolutionResult::kSettingDisabled;
  }
  const bool auth_tokens_are_available = ipp_core_->AreAuthTokensAvailable();
  const bool proxy_list_is_available = ipp_core_->IsProxyListAvailable();
  if (!auth_tokens_are_available && !proxy_list_is_available) {
    vlog("neither proxy list nor auth token available");
    return ProxyResolutionResult::kTokensAndProxyListNotAvailable;
  } else if (!auth_tokens_are_available) {
    vlog("no auth token available from cache");
    return ProxyResolutionResult::kTokensNotAvailable;
  } else if (!proxy_list_is_available) {
    vlog("no proxy list available from cache");
    return ProxyResolutionResult::kProxyListNotAvailable;
  }

  return ProxyResolutionResult::kAttemptProxy;
}

void IpProtectionProxyDelegate::OnResolveProxy(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  ProxyResolutionResult resolution_result =
      ClassifyRequest(url, network_anonymization_key, result);
  Telemetry().ProxyResolution(resolution_result);
  if (resolution_result != ProxyResolutionResult::kAttemptProxy) {
    return;
  }

  net::ProxyList proxy_list;
  if (!net::features::kIpPrivacyDirectOnly.Get()) {
    const std::vector<net::ProxyChain>& proxy_chain_list =
        ipp_core_->GetProxyChainList();
    for (const auto& proxy_chain : proxy_chain_list) {
      // Proxying HTTP traffic over HTTPS/SPDY proxies requires multi-proxy
      // chains.
      CHECK(proxy_chain.is_multi_proxy());

      // For debugging..
      if (net::features::kIpPrivacyUseSingleProxy.Get()) {
        proxy_list.AddProxyChain(net::ProxyChain::ForIpProtection({
            proxy_chain.GetProxyServer(0),
        }));
      } else {
        proxy_list.AddProxyChain(std::move(proxy_chain));
      }
    }
  }

  if (net::features::kIpPrivacyFallbackToDirect.Get()) {
    // Final fallback is to DIRECT.
    auto direct_proxy_chain = net::ProxyChain::ForIpProtection({});
    proxy_list.AddProxyChain(std::move(direct_proxy_chain));
  }

  if (VLOG_IS_ON(3)) {
    std::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    VLOG(3) << "IPPD::OnResolveProxy(" << url << ", "
            << (top_frame_site.has_value() ? top_frame_site.value()
                                           : net::SchemefulSite())
            << ") - setting proxy list (before deprioritization) to "
            << proxy_list.ToDebugString();
  }
  result->OverrideProxyList(MergeProxyRules(result->proxy_list(), proxy_list));
  result->DeprioritizeBadProxyChains(proxy_retry_info);
  return;
}

void IpProtectionProxyDelegate::OnSuccessfulRequestAfterFailures(
    const net::ProxyRetryInfoMap& proxy_retry_info) {
  if (!ipp_core_) {
    return;
  }

  // A request was successful, but one or more proxies failed. If _only_ QUIC
  // proxies failed, then we assume this is because QUIC is not working on
  // this network, and stop injecting QUIC proxies into the proxy list.
  bool seen_quic = false;
  for (const auto& chain_and_info : proxy_retry_info) {
    const net::ProxyChain& proxy_chain = chain_and_info.first;
    if (!proxy_chain.is_for_ip_protection()) {
      continue;
    }
    const net::ProxyServer& proxy_server = proxy_chain.First();
    if (proxy_server.is_quic()) {
      seen_quic = true;
    } else {
      // A non-QUIC chain has failed.
      return;
    }
  }

  if (seen_quic) {
    // Only QUIC chains failed.
    ipp_core_->QuicProxiesFailed();
  }
}

void IpProtectionProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                           int net_error) {
  // If the bad proxy was an IP Protection proxy, refresh the list of IP
  // protection proxies immediately.
  if (bad_chain.is_for_ip_protection()) {
    Telemetry().ProxyChainFallback(bad_chain.ip_protection_chain_id());
    ipp_core_->RequestRefreshProxyList();
  }
}

net::Error IpProtectionProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    net::HttpRequestHeaders* extra_headers) {
  auto vlog = [](std::string message) {
    VLOG(2) << "NSPD::OnBeforeTunnelRequest() - " << message;
  };
  if (proxy_chain.is_for_ip_protection()) {
    std::optional<BlindSignedAuthToken> token =
        ipp_core_->GetAuthToken(chain_index);
    if (token) {
      vlog("adding auth token");
      // The token value we have here is the full Authorization header value,
      // so we can add it verbatim.
      extra_headers->SetHeader(net::HttpRequestHeaders::kAuthorization,
                               std::move(token->token));
    } else {
      vlog("no token available");
      // This is an unexpected circumstance, but does happen in the wild.
      // Rather than send the request to the proxy, which will reply with an
      // error, mark the connection as failed immediately.
      return net::ERR_TUNNEL_CONNECTION_FAILED;
    }
  } else {
    vlog("not for IP protection");
  }
  int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
  if (experiment_arm != 0) {
    extra_headers->SetHeader("Ip-Protection-Debug-Experiment-Arm",
                             base::NumberToString(experiment_arm));
  }
  return net::OK;
}

net::Error IpProtectionProxyDelegate::OnTunnelHeadersReceived(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    const net::HttpResponseHeaders& response_headers) {
  return net::OK;
}

void IpProtectionProxyDelegate::SetProxyResolutionService(
    net::ProxyResolutionService* proxy_resolution_service) {}

// static
net::ProxyList IpProtectionProxyDelegate::MergeProxyRules(
    const net::ProxyList& existing_proxy_list,
    const net::ProxyList& custom_proxy_list) {
  net::ProxyList merged_proxy_list;
  for (const auto& existing_chain : existing_proxy_list.AllChains()) {
    if (existing_chain.is_direct()) {
      // Replace direct option with all proxies in the custom proxy list
      for (const auto& custom_chain : custom_proxy_list.AllChains()) {
        merged_proxy_list.AddProxyChain(custom_chain);
      }
    } else {
      merged_proxy_list.AddProxyChain(existing_chain);
    }
  }

  return merged_proxy_list;
}

}  // namespace ip_protection
