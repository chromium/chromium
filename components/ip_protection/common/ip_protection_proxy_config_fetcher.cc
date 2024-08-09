// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_fetcher.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "components/ip_protection/common/ip_protection_proxy_config_retriever.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace ip_protection {

IpProtectionProxyConfigFetcher::IpProtectionProxyConfigFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string type,
    std::string api_key) {
  CHECK(url_loader_factory);
  ip_protection_proxy_config_retriever_ =
      std::make_unique<IpProtectionProxyConfigRetriever>(url_loader_factory,
                                                         type, api_key);
}

IpProtectionProxyConfigFetcher::IpProtectionProxyConfigFetcher(
    std::unique_ptr<IpProtectionProxyConfigRetriever>
        ip_protection_proxy_config_retriever)
    : ip_protection_proxy_config_retriever_(
          std::move(ip_protection_proxy_config_retriever)) {}

IpProtectionProxyConfigFetcher::~IpProtectionProxyConfigFetcher() = default;

void IpProtectionProxyConfigFetcher::CallGetProxyConfig(
    GetProxyListCallback callback,
    std::optional<std::string> oauth_token) {
  ip_protection_proxy_config_retriever_->GetProxyConfig(
      oauth_token,
      base::BindOnce(&IpProtectionProxyConfigFetcher::OnGetProxyConfigCompleted,
                     base::Unretained(this), std::move(callback)));
}

void IpProtectionProxyConfigFetcher::OnGetProxyConfigCompleted(
    GetProxyListCallback callback,
    base::expected<ip_protection::GetProxyConfigResponse, std::string>
        response) {
  // If either there is an empty response or no geo hint present, it should be
  // treated as an error and cause a retry.
  if (IsProxyConfigResponseError(response)) {
    VLOG(2) << "IpProtectionProxyConfigFetcher::CallGetProxyConfig failed: "
            << response.error();

    // Apply exponential backoff to this sort of failure.
    no_get_proxy_config_until_ =
        base::Time::Now() + next_get_proxy_config_backoff_;
    next_get_proxy_config_backoff_ *= 2;

    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  // Cancel any backoff on success.
  no_get_proxy_config_until_ = base::Time();
  next_get_proxy_config_backoff_ = kGetProxyConfigFailureTimeout;

  std::vector<net::ProxyChain> proxy_list =
      GetProxyListFromProxyConfigResponse(response.value());
  std::optional<network::GeoHint> geo_hint =
      GetGeoHintFromProxyConfigResponse(response.value());
  std::move(callback).Run(std::move(proxy_list), std::move(geo_hint));
}

bool IpProtectionProxyConfigFetcher::IsProxyConfigResponseError(
    const base::expected<ip_protection::GetProxyConfigResponse, std::string>&
        response) {
  if (!response.has_value()) {
    return true;
  }

  // Returns true for an error when a geo hint is missing but is required b/c
  // the proxy chain is NOT empty.
  const ip_protection::GetProxyConfigResponse& config_response =
      response.value();
  return !config_response.has_geo_hint() &&
         !config_response.proxy_chain().empty();
}

std::vector<net::ProxyChain>
IpProtectionProxyConfigFetcher::GetProxyListFromProxyConfigResponse(
    ip_protection::GetProxyConfigResponse response) {
  // Shortcut to create a ProxyServer with SCHEME_HTTPS from a string in the
  // proto.
  auto add_server = [](std::vector<net::ProxyServer>& proxies,
                       std::string host) {
    net::ProxyServer proxy_server = net::ProxySchemeHostAndPortToProxyServer(
        net::ProxyServer::SCHEME_HTTPS, host);
    if (!proxy_server.is_valid()) {
      return false;
    }
    proxies.push_back(proxy_server);
    return true;
  };

  std::vector<net::ProxyChain> proxy_list;
  for (const auto& proxy_chain : response.proxy_chain()) {
    std::vector<net::ProxyServer> proxies;
    bool ok = true;
    bool overridden = false;
    if (const std::string a_override =
            net::features::kIpPrivacyProxyAHostnameOverride.Get();
        a_override != "") {
      overridden = true;
      ok = ok && add_server(proxies, a_override);
    } else {
      ok = ok && add_server(proxies, proxy_chain.proxy_a());
    }
    if (const std::string b_override =
            net::features::kIpPrivacyProxyBHostnameOverride.Get();
        ok && b_override != "") {
      overridden = true;
      ok = ok && add_server(proxies, b_override);
    } else {
      ok = ok && add_server(proxies, proxy_chain.proxy_b());
    }

    // Create a new ProxyChain if the proxies were all valid.
    if (ok) {
      // If the `chain_id` is out of range or local features overrode the
      // chain, use the proxy chain anyway, but with the default `chain_id`.
      // This allows adding new IDs on the server side without breaking older
      // browsers.
      int chain_id = proxy_chain.chain_id();
      if (overridden || chain_id < 0 ||
          chain_id > net::ProxyChain::kMaxIpProtectionChainId) {
        chain_id = net::ProxyChain::kDefaultIpProtectionChainId;
      }
      proxy_list.push_back(
          net::ProxyChain::ForIpProtection(std::move(proxies), chain_id));
    }
  }

  VLOG(2) << "IPATP::GetProxyList got proxy list of length "
          << proxy_list.size();

  return proxy_list;
}

std::optional<network::GeoHint>
IpProtectionProxyConfigFetcher::GetGeoHintFromProxyConfigResponse(
    ip_protection::GetProxyConfigResponse& response) {
  if (!response.has_geo_hint()) {
    return std::nullopt;  // No GeoHint available in the response.
  }

  return std::make_optional<network::GeoHint>(
      {.country_code = response.geo_hint().country_code(),
       .iso_region = response.geo_hint().iso_region(),
       .city_name = response.geo_hint().city_name()});
}

void IpProtectionProxyConfigFetcher::SetUpForTesting(
    std::unique_ptr<IpProtectionProxyConfigRetriever>
        ip_protection_proxy_config_retriever) {
  ip_protection_proxy_config_retriever_ = nullptr;

  ip_protection_proxy_config_retriever_ =
      std::move(ip_protection_proxy_config_retriever);
}

net::ProxyChain IpProtectionProxyConfigFetcher::MakeChainForTesting(
    std::vector<std::string> hostnames,
    int chain_id) {
  std::vector<net::ProxyServer> servers;
  for (auto& hostname : hostnames) {
    servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
        net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
  }
  return net::ProxyChain::ForIpProtection(servers, chain_id);
}

}  // namespace ip_protection
