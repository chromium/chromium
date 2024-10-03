// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_DIRECT_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_DIRECT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_fetcher.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/base/proxy_chain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ip_protection {

// Manages fetching the proxy configuration from the server that is
// necessary for IP Protection.
//
// This class is responsible for using the retriever to get the proxy config,
// retrying if necessary, and creating the corresponding ProxyChain list and
// GeoHint.
class IpProtectionProxyConfigDirectFetcher
    : public IpProtectionProxyConfigFetcher {
 public:
  // Callback made when an AuthenticateCallback is done. The callback's boolean
  // parameter is true on success, and false if the request should not be made.
  // The `ResourceRequest` is the request passed to `AuthenticateCallback`,
  // possibly modified to include authentication information.
  using AuthenticateDoneCallback =
      base::OnceCallback<void(bool, std::unique_ptr<network::ResourceRequest>)>;

  // A callback to add any necessary authentication information to the given
  // `ResourceRequest` and invoke the given callback. The ResourceRequest
  // remains valid until the donecallback is invoked. If AuthenticateCallback is
  // null, the request will be made with no authentication information.
  using AuthenticateCallback =
      base::RepeatingCallback<void(std::unique_ptr<network::ResourceRequest>,
                                   AuthenticateDoneCallback)>;

  // Retrieve proxy config from an HTTP server, abstracted to ease testing of
  // the fetcher.
  class Retriever {
   public:
    explicit Retriever(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::string service_type,
        AuthenticateCallback authenticate_callback);
    virtual ~Retriever();
    using RetrieveCallback = base::OnceCallback<void(
        base::expected<GetProxyConfigResponse, std::string>)>;
    virtual void RetrieveProxyConfig(RetrieveCallback callback);

   private:
    void OnAuthenticatedResourceRequest(
        RetrieveCallback callback,
        bool success,
        std::unique_ptr<network::ResourceRequest> resource_request);

    void OnGetProxyConfigCompleted(
        std::unique_ptr<network::SimpleURLLoader> url_loader,
        RetrieveCallback callback,
        std::unique_ptr<std::string> response);

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    const GURL ip_protection_server_url_;
    const std::string ip_protection_server_get_proxy_config_path_;
    const std::string service_type_;
    AuthenticateCallback authenticate_callback_;
    base::WeakPtrFactory<Retriever> weak_ptr_factory_{this};
  };

  explicit IpProtectionProxyConfigDirectFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string service_type,
      AuthenticateCallback authenticate_callback);

  explicit IpProtectionProxyConfigDirectFetcher(
      std::unique_ptr<Retriever> retriever);

  ~IpProtectionProxyConfigDirectFetcher() override;

  // IpProtectionProxyConfigFetcher implementation.
  void GetProxyConfig(GetProxyConfigCallback callback) override;

  // Shortcut to create a ProxyChain from hostnames for unit tests.
  static net::ProxyChain MakeChainForTesting(
      std::vector<std::string> hostnames,
      int chain_id = net::ProxyChain::kDefaultIpProtectionChainId);

  base::Time GetNoGetProxyConfigUntilTime() {
    return no_get_proxy_config_until_;
  }

  // Timeout for failures from GetProxyConfig. This is doubled for
  // each subsequent failure.
  static constexpr base::TimeDelta kGetProxyConfigFailureTimeout =
      base::Minutes(1);

 private:
  void OnGetProxyConfigCompleted(
      GetProxyConfigCallback callback,
      base::expected<GetProxyConfigResponse, std::string> response);

  // Returns true if the GetProxyConfigResponse contains an error or is invalid.
  // In order for a response to be valid, the following must be true:
  //    1. !response.has_value()
  //    2. If a response has a value and the proxy chain is NOT empty and the
  //       GeoHint MUST be present.
  bool IsProxyConfigResponseError(
      const base::expected<GetProxyConfigResponse, std::string>& response);

  // Creates a list of ProxyChains from GetProxyConfigResponse.
  std::vector<net::ProxyChain> GetProxyListFromProxyConfigResponse(
      GetProxyConfigResponse response);

  // Creates a GeoHint by converting the GeoHint from the
  // `GetProxyConfigResponse` to a `network::GeoHint`.
  std::optional<GeoHint> GetGeoHintFromProxyConfigResponse(
      GetProxyConfigResponse& response);

  std::unique_ptr<Retriever> ip_protection_proxy_config_retriever_;

  // The time before the retriever's GetProxyConfig should not be called, and
  // the exponential backoff to be applied next time such a call fails.
  base::Time no_get_proxy_config_until_;
  base::TimeDelta next_get_proxy_config_backoff_ =
      kGetProxyConfigFailureTimeout;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_DIRECT_FETCHER_H_
