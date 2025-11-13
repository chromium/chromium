// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_delegate.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager_impl.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/gurl.h"

namespace ip_protection {

IpProtectionProxyDelegate::IpProtectionProxyDelegate(
    IpProtectionCore* ip_protection_core)
    : ip_protection_core_(
          raw_ref<IpProtectionCore>::from_ptr(ip_protection_core)) {}

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

  bool is_unconditional_match = false;
  if (std::string domain_list_str =
          net::features::kIpPrivacyUnconditionalProxyDomainList.Get();
      !domain_list_str.empty()) {
    std::vector<std::string> domain_list = base::SplitString(
        domain_list_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    if (top_frame_site.has_value()) {
      for (const auto& domain : domain_list) {
        // SchemefulSite normalizes to eTLD+1 using the Public Suffix List.
        std::string registrable_domain = top_frame_site->GetURL().GetHost();
        if (registrable_domain == domain) {
          vlog("unconditional proxy domain matched");
          is_unconditional_match = true;
          break;
        }
      }
    }
  }

  if (!is_unconditional_match) {
    // Check eligibility of this request.
    if (!ip_protection_core_->IsMdlPopulated()) {
      vlog("proxy allow list not populated");
      return ProxyResolutionResult::kMdlNotPopulated;
    } else if (!ip_protection_core_->RequestShouldBeProxied(
                   url, network_anonymization_key)) {
      vlog("proxy allow list did not match");
      return ProxyResolutionResult::kNoMdlMatch;
    } else {
      vlog("proxy allow list matched");
    }
  }

  result->set_is_mdl_match(true);

  // Check availability. We do not proxy requests if:
  // - The allow list has not been populated.
  // - The request doesn't match the allow list.
  // - The token cache is not available.
  // - The token cache does not have tokens.
  // - No proxy list is available.
  // - `is_ip_protection_enabled_` is `false` (in other words, the user has
  //   disabled IP Protection via user settings or enterprise policy).
  // - `kIpPrivacyDirectOnly` is `true`.
  if (!ip_protection_core_->IsIpProtectionEnabled()) {
    vlog("ip protection proxy is not currently enabled");
    return ProxyResolutionResult::kSettingDisabled;
  }
  const bool were_token_caches_ever_filled =
      ip_protection_core_->WereTokenCachesEverFilled();
  const bool auth_tokens_are_available =
      ip_protection_core_->AreAuthTokensAvailable();
  const bool proxy_list_is_available =
      ip_protection_core_->IsProxyListAvailable();

  if (!proxy_list_is_available) {
    vlog("no proxy list available from cache");
    return ProxyResolutionResult::kProxyListNotAvailable;
  } else if (!were_token_caches_ever_filled) {
    vlog("token caches have never been filled");
    return ProxyResolutionResult::kTokensNeverAvailable;
  } else if (!auth_tokens_are_available) {
    vlog("no auth token available from cache");
    // Signal demand for both proxy layers. The respective token managers can
    // determine whether a token fetch is ongoing or not.
    ip_protection_core_->RecordTokenDemand(/*chain_index=*/0);
    ip_protection_core_->RecordTokenDemand(/*chain_index=*/1);
    return ProxyResolutionResult::kTokensExhausted;
  }

  // Check if the protection has been disabled via User Bypass.
  if (net::features::kIpPrivacyEnableUserBypass.Get() &&
      ip_protection_core_->HasTrackingProtectionException(
          network_anonymization_key.GetTopFrameSite()->GetURL())) {
    return ProxyResolutionResult::kHasSiteException;
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
  // Don't emit the ProxyResolution metric if unconditional proxying is enabled,
  // since it can skew common IPP analyses.
  // TODO(crbug.com/447391924) - Rework this metric so we can support
  // unconditional proxying as well.
  if (net::features::kIpPrivacyUnconditionalProxyDomainList.Get().empty()) {
    Telemetry().ProxyResolution(resolution_result);
  }

  const std::optional<net::SchemefulSite>& top_frame_site =
      network_anonymization_key.GetTopFrameSite();
  if (resolution_result != ProxyResolutionResult::kAttemptProxy) {
    return;
  }

  net::ProxyList proxy_list;
  if (!net::features::kIpPrivacyDirectOnly.Get()) {
    const std::vector<net::ProxyChain>& proxy_chain_list =
        ip_protection_core_->GetProxyChainList();
    for (const auto& proxy_chain : proxy_chain_list) {
      // Proxying HTTP traffic over HTTPS/SPDY proxies requires multi-proxy
      // chains.
      CHECK(proxy_chain.is_multi_proxy());

      proxy_list.AddProxyChain(std::move(proxy_chain));
    }
  }

  if (net::features::kIpPrivacyFallbackToDirect.Get()) {
    // Final fallback is to DIRECT.
    auto direct_proxy_chain = net::ProxyChain::ForIpProtection({});
    proxy_list.AddProxyChain(std::move(direct_proxy_chain));
  }

  if (VLOG_IS_ON(3)) {
    VLOG(3) << "IPPD::OnResolveProxy(" << url << ", "
            << (top_frame_site.has_value() ? top_frame_site.value()
                                           : net::SchemefulSite())
            << ") - setting proxy list (before deprioritization) to "
            << proxy_list.ToDebugString();
  }

  if (!net::features::kIpPrivacyDirectOnly.Get()) {
    proxy_list.DeprioritizeBadProxyChains(proxy_retry_info,
                                          /*remove_bad_proxy_chains=*/true);
    if (proxy_list.IsEmpty()) {
      return;
    }
    // Two cases are possible here:
    //   1. All IPP Proxy Chains were marked as bad.
    //   2. IPPCore returned no chains.
    //
    // In either case, using a proxy chain where is_for_ip_protection() is true
    // is misleading since IPP is not used at all.
    if (proxy_list.First().is_direct()) {
      VLOG(3) << "IPPD::OnResolveProxy(" << url << ", "
              << (top_frame_site.has_value() ? top_frame_site.value()
                                             : net::SchemefulSite())
              << ") - all proxy chains deprioritized: "
              << proxy_list.ToDebugString();
      return;
    }
  }

  result->OverrideProxyList(MergeProxyRules(result->proxy_list(), proxy_list));
  result->DeprioritizeBadProxyChains(proxy_retry_info);
  return;
}

void IpProtectionProxyDelegate::OnSuccessfulRequestAfterFailures(
    const net::ProxyRetryInfoMap& proxy_retry_info) {
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
    ip_protection_core_->QuicProxiesFailed();
  }
}

void IpProtectionProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                           int net_error) {
  // If the bad proxy was an IP Protection proxy, refresh the list of IP
  // protection proxies immediately.
  if (bad_chain.is_for_ip_protection()) {
    Telemetry().ProxyChainFallback(bad_chain.ip_protection_chain_id());
    ip_protection_core_->RequestRefreshProxyList();
  }
}

base::expected<net::HttpRequestHeaders, net::Error>
IpProtectionProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t proxy_index,
    OnBeforeTunnelRequestCallback callback) {
  auto vlog = [](std::string message) {
    VLOG(2) << "NSPD::OnBeforeTunnelRequest() - " << message;
  };
  net::HttpRequestHeaders extra_headers;
  if (proxy_chain.is_for_ip_protection()) {
    ip_protection_core_->RecordTokenDemand(proxy_index);
    std::optional<BlindSignedAuthToken> token =
        ip_protection_core_->GetAuthToken(proxy_index);
    if (token) {
      vlog("adding auth token");
      // The token value we have here is the full Authorization header value,
      // so we can add it verbatim.
      extra_headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                              std::move(token->token));
    } else {
      vlog("no token available");
      // This is an unexpected circumstance, but does happen in the wild.
      // Rather than send the request to the proxy, which will reply with an
      // error, mark the connection as failed immediately.
      return base::unexpected(net::ERR_TUNNEL_CONNECTION_FAILED);
    }
    int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
    if (experiment_arm != 0) {
      extra_headers.SetHeader("Ip-Protection-Debug-Experiment-Arm",
                              base::NumberToString(experiment_arm));
    }
  } else {
    vlog("not for IP protection");
  }
  return extra_headers;
}

net::Error IpProtectionProxyDelegate::OnTunnelHeadersReceived(
    const net::ProxyChain& proxy_chain,
    size_t proxy_index,
    const net::HttpResponseHeaders& response_headers,
    net::CompletionOnceCallback callback) {
  if (response_headers.response_code() == 200 ||
      !proxy_chain.is_for_ip_protection()) {
    return net::OK;
  }

  if (!base::FeatureList::IsEnabled(
          net::features::kEnableIpPrivacyProxyAdvancedFallbackLogic)) {
    return net::OK;
  }

  std::optional<std::string> proxy_status_header_value =
      response_headers.GetNormalizedHeader("Proxy-Status");
  if (!proxy_status_header_value) {
    return net::OK;
  }

  std::optional<net::structured_headers::List> proxy_status_list =
      net::structured_headers::ParseList(*proxy_status_header_value);
  if (!proxy_status_list) {
    return net::OK;
  }

  net::structured_headers::List parsed_list = proxy_status_list.value();
  // For IP Protection there will only ever be one proxy server per connection,
  // so there should only ever be one element in the list corresponding to that
  // proxy server. Even for the connection to Proxy B, this request is not
  // visible by Proxy A and thus the Proxy-Status header can't be modified by
  // it.
  if (parsed_list.size() != 1) {
    return net::OK;
  }
  const net::structured_headers::ParameterizedMember& p_member = parsed_list[0];
  // `p_member` can either be a single Item or an inner list, and we expect the
  // format here for this Proxy-Status header to be considered valid.
  if (p_member.member_is_inner_list) {
    return net::OK;
  }

  bool error_is_dns_error = false;
  bool rcode_is_nxdomain_or_nodata = false;
  for (const auto& [name, item] : p_member.params) {
    if (name == "error" && item.is_token()) {
      static constexpr auto kDestinationErrors =
          base::MakeFixedFlatSet<std::string_view>({
              "destination_not_found",
              "destination_unavailable",
              "destination_ip_unroutable",
              "connection_refused",
              "connection_terminated",
              "connection_timeout",
              "proxy_loop_detected",
          });
      const std::string& error_val = item.GetString();
      // These RFC 9209 errors indicate a destination-side problem.
      // For these, we should NOT fall back.
      if (kDestinationErrors.contains(error_val)) {
        return net::ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION;
      }
      if (error_val == "dns_error") {
        error_is_dns_error = true;
      }
      continue;
    }

    if (name == "rcode" && item.is_string()) {
      const std::string& rcode_val = item.GetString();
      if (rcode_val == "NXDOMAIN" || rcode_val == "NODATA") {
        rcode_is_nxdomain_or_nodata = true;
      }
      continue;
    }
  }
  if (error_is_dns_error && rcode_is_nxdomain_or_nodata) {
    return net::ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION;
  }

  // If no specific destination error was found, we assume it's a proxy
  // failure and should fall back.
  return net::OK;
}

void IpProtectionProxyDelegate::SetProxyResolutionService(
    net::ProxyResolutionService* proxy_resolution_service) {}

bool IpProtectionProxyDelegate::AliasRequiresProxyOverride(
    const std::string scheme,
    const std::vector<std::string>& dns_aliases,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  // TODO(crbug.com/383134117): Iterate through aliases and invoke mdl manager
  // to check if any match a 3p resource.
  return false;
}

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

void IpProtectionProxyDelegate::OnStreamCreationAttempted(
    const net::ProxyChain& proxy_chain,
    base::TimeDelta duration,
    base::optional_ref<int> net_error) {
  if (!proxy_chain.is_for_ip_protection()) {
    return;
  }

  Telemetry().RecordStreamCreationAttemptedMetrics(proxy_chain, duration,
                                                   net_error);
}

}  // namespace ip_protection
