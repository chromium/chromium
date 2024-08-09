// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "components/ip_protection/common/ip_protection_proxy_config_retriever.h"
#include "net/base/proxy_chain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ip_protection {

// Manages fetching the proxy configuration from the server that is
// necessary for IP Protection.
//
// This class is responsible for using the retriever to get the proxy config,
// retrying if necessary, and creating the corresponding ProxyChain list and
// GeoHint.
class IpProtectionProxyConfigFetcher {
 public:
  using GetProxyListCallback = base::OnceCallback<void(
      const std::optional<std::vector<::net::ProxyChain>>&,
      const std::optional<network::GeoHint>&)>;

  explicit IpProtectionProxyConfigFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string type,
      std::string api_key);

  explicit IpProtectionProxyConfigFetcher(
      std::unique_ptr<IpProtectionProxyConfigRetriever>
          ip_protection_proxy_config_retriever);

  ~IpProtectionProxyConfigFetcher();

  // Get proxy configuration that is necessary for IP Protection from the
  // server.
  void CallGetProxyConfig(GetProxyListCallback callback,
                          std::optional<std::string> oauth_token);

  // Shortcut to create a ProxyChain from hostnames for unit tests.
  static net::ProxyChain MakeChainForTesting(
      std::vector<std::string> hostnames,
      int chain_id = net::ProxyChain::kDefaultIpProtectionChainId);

  base::Time GetNoGetProxyConfigUntilTime() {
    return no_get_proxy_config_until_;
  }

  void SetUpForTesting(std::unique_ptr<IpProtectionProxyConfigRetriever>
                           ip_protection_proxy_config_retriever);

  // Timeout for failures from GetProxyConfig. This is doubled for
  // each subsequent failure.
  static constexpr base::TimeDelta kGetProxyConfigFailureTimeout =
      base::Minutes(1);

 private:
  void OnGetProxyConfigCompleted(
      GetProxyListCallback callback,
      base::expected<ip_protection::GetProxyConfigResponse, std::string>
          response);

  // Returns true if the GetProxyConfigResponse contains an error or is invalid.
  // In order for a response to be valid, the following must be true:
  //    1. !response.has_value()
  //    2. If a response has a value and the proxy chain is NOT empty and the
  //       GeoHint MUST be present.
  bool IsProxyConfigResponseError(
      const base::expected<ip_protection::GetProxyConfigResponse, std::string>&
          response);

  // Creates a list of ProxyChains from GetProxyConfigResponse.
  std::vector<net::ProxyChain> GetProxyListFromProxyConfigResponse(
      ip_protection::GetProxyConfigResponse response);

  // Creates a GeoHint by converting the GeoHint from the
  // `GetProxyConfigResponse` to a `network::GeoHint`.
  std::optional<network::GeoHint> GetGeoHintFromProxyConfigResponse(
      ip_protection::GetProxyConfigResponse& response);

  std::unique_ptr<IpProtectionProxyConfigRetriever>
      ip_protection_proxy_config_retriever_;

  // The time before the retriever's GetProxyConfig should not be called, and
  // the exponential backoff to be applied next time such a call fails.
  base::Time no_get_proxy_config_until_;
  base::TimeDelta next_get_proxy_config_backoff_ =
      kGetProxyConfigFailureTimeout;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_FETCHER_H_
