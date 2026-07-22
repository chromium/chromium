// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_PROVISIONING_DOMAIN_FETCHER_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_PROVISIONING_DOMAIN_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/provisioning_domain_client.h"
#include "components/enterprise/net/core/types.h"
#include "net/http/http_request_headers.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_net {

class EnterpriseNetworkAuthService;
class ProvisioningDomainClient;

// LINT.IfChange(EnterpriseProvisioningDomainFetchResult)
enum class ProvisioningDomainFetchResultStatus {
  kSuccess = 0,
  kInvalidUrl = 1,
  kTokenFetchError = 2,
  kHttpError = 3,
  kParseError = 4,
  kMaxValue = kParseError,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:EnterpriseProvisioningDomainFetchResult)

// Detailed error state returned when a Provisioning Domain configuration fetch
// fails.
struct ProvisioningDomainFetchError {
  explicit ProvisioningDomainFetchError(
      ProvisioningDomainFetchResultStatus status);
  ProvisioningDomainFetchError(const ProvisioningDomainFetchError&);
  ProvisioningDomainFetchError(ProvisioningDomainFetchError&&) noexcept;
  ProvisioningDomainFetchError& operator=(const ProvisioningDomainFetchError&);
  ProvisioningDomainFetchError& operator=(
      ProvisioningDomainFetchError&&) noexcept;
  ~ProvisioningDomainFetchError();

  ProvisioningDomainFetchResultStatus status;

  // Populated for TokenFetchError.
  std::optional<TokenFetchError> token_fetch_error;

  // Populated for:
  // - InvalidUrl (in particular, net::ERR_INVALID_URL), and
  // - HttpError (net error code from network stack).
  int net_error = 0;

  // Populated for HttpError (HTTP status code, e.g. 404, 500).
  std::optional<int> response_code;

  bool operator==(const ProvisioningDomainFetchError&) const = default;
};

using ProvisioningDomainFetchResult =
    base::expected<ProvisioningDomainProxyConfig, ProvisioningDomainFetchError>;

// Dedicated fetch orchestrator that executes a Provisioning Domain (PvD)
// fetch lifecycle, from PvD authentication, to PvD GET call and finally
// JSON response parsing. Also provides detailed error information should one
// occur.
class ProvisioningDomainFetcher : public ProvisioningDomainClient::Delegate {
 public:
  using FetchCompleteCallback =
      base::OnceCallback<void(ProvisioningDomainFetchResult result)>;

  ProvisioningDomainFetcher(
      const ProvisioningDomainConfig& policy_config,
      EnterpriseNetworkAuthService* auth_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ProvisioningDomainFetcher(const ProvisioningDomainFetcher&) = delete;
  ProvisioningDomainFetcher& operator=(const ProvisioningDomainFetcher&) =
      delete;
  ~ProvisioningDomainFetcher() override;

  // ProvisioningDomainClient::Delegate:
  net::HttpRequestHeaders GetExtraHeaders() const override;

  // Initiates the fetch workflow for `policy_config_`.
  // If a fetch is already in-flight, queues `callback` to receive the result
  // when complete.
  void Start(FetchCompleteCallback callback);

  // Cancels any active fetch workflow.
  void Cancel();

  // Returns true if a fetch is currently in-flight.
  bool is_fetching() const { return !pending_callbacks_.empty(); }

  const ProvisioningDomainConfig& policy_config() const {
    return policy_config_;
  }

 private:
  void OnAccessTokenFetched(AccessTokenResult access_token_result);
  void OnHttpFetchComplete(ProvisioningDomainClientResult client_result);
  void CompleteFetch(const ProvisioningDomainFetchResult& result);

  const ProvisioningDomainConfig policy_config_;
  const raw_ptr<EnterpriseNetworkAuthService> auth_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ProvisioningDomainClient> client_;

  std::vector<FetchCompleteCallback> pending_callbacks_;

  base::WeakPtrFactory<ProvisioningDomainFetcher> weak_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_PROVISIONING_DOMAIN_FETCHER_H_
