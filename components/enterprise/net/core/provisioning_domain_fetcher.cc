// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/provisioning_domain_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/provisioning_domain_client.h"
#include "components/enterprise/net/core/utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_net {

namespace {

constexpr char kWellKnownPvdPath[] = "/.well-known/pvd";

}  // namespace

ProvisioningDomainFetchError::ProvisioningDomainFetchError(
    ProvisioningDomainFetchResultStatus status)
    : status(status) {}
ProvisioningDomainFetchError::ProvisioningDomainFetchError(
    const ProvisioningDomainFetchError&) = default;
ProvisioningDomainFetchError::ProvisioningDomainFetchError(
    ProvisioningDomainFetchError&&) noexcept = default;
ProvisioningDomainFetchError& ProvisioningDomainFetchError::operator=(
    const ProvisioningDomainFetchError&) = default;
ProvisioningDomainFetchError& ProvisioningDomainFetchError::operator=(
    ProvisioningDomainFetchError&&) noexcept = default;
ProvisioningDomainFetchError::~ProvisioningDomainFetchError() = default;

ProvisioningDomainFetcher::ProvisioningDomainFetcher(
    const ProvisioningDomainConfig& policy_config,
    EnterpriseNetworkAuthService* auth_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : policy_config_(policy_config),
      auth_service_(auth_service),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(auth_service_);
  CHECK(url_loader_factory_);
}

ProvisioningDomainFetcher::~ProvisioningDomainFetcher() = default;

net::HttpRequestHeaders ProvisioningDomainFetcher::GetExtraHeaders() const {
  return auth_service_->ResolveExtraHeaders(policy_config_.extra_headers);
}

void ProvisioningDomainFetcher::Start(FetchCompleteCallback callback) {
  pending_callbacks_.push_back(std::move(callback));

  // If a fetch is already in-flight for this fetcher, queue the callback.
  if (pending_callbacks_.size() > 1) {
    return;
  }

  GURL base_url(base::StrCat({"https://", policy_config_.pvd_id}));
  GURL url = base_url.Resolve(kWellKnownPvdPath);
  if (!url.is_valid()) {
    ProvisioningDomainFetchError error(
        ProvisioningDomainFetchResultStatus::kInvalidUrl);
    error.net_error = net::ERR_INVALID_URL;
    CompleteFetch(base::unexpected(std::move(error)));
    return;
  }

  // Unsupported `AuthType`s are not configurable by the policy, so we don't
  // need to handle it here.
  if (policy_config_.auth_config.has_value() &&
      policy_config_.auth_config->type == AuthType::kProfileBearerToken) {
    // TODO(crbug.com/535229810): Enforce scope restrictions based on the
    // domain.
    auth_service_->FetchAccessToken(
        policy_config_.auth_config->scope,
        base::BindOnce(&ProvisioningDomainFetcher::OnAccessTokenFetched,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // No OAuth authentication required; proceed directly to HTTP fetch.
  client_ = std::make_unique<ProvisioningDomainClient>(url, this,
                                                       url_loader_factory_);
  client_->Fetch(
      /*access_token=*/std::nullopt,
      base::BindOnce(&ProvisioningDomainFetcher::OnHttpFetchComplete,
                     weak_factory_.GetWeakPtr()));
}

void ProvisioningDomainFetcher::Cancel() {
  weak_factory_.InvalidateWeakPtrs();
  pending_callbacks_.clear();
  if (client_) {
    client_->Cancel();
  }
}

void ProvisioningDomainFetcher::OnAccessTokenFetched(
    AccessTokenResult access_token_result) {
  if (!access_token_result.has_value()) {
    ProvisioningDomainFetchError error(
        ProvisioningDomainFetchResultStatus::kTokenFetchError);
    error.token_fetch_error = access_token_result.error();
    CompleteFetch(base::unexpected(std::move(error)));
    return;
  }

  GURL base_url(base::StrCat({"https://", policy_config_.pvd_id}));
  GURL url = base_url.Resolve(kWellKnownPvdPath);
  client_ = std::make_unique<ProvisioningDomainClient>(url, this,
                                                       url_loader_factory_);
  client_->Fetch(*access_token_result,
                 base::BindOnce(&ProvisioningDomainFetcher::OnHttpFetchComplete,
                                weak_factory_.GetWeakPtr()));
}

void ProvisioningDomainFetcher::OnHttpFetchComplete(
    ProvisioningDomainClientResult client_result) {
  if (!client_result.has_value()) {
    ProvisioningDomainFetchError error(
        ProvisioningDomainFetchResultStatus::kHttpError);
    error.net_error = client_result.error().net_error;
    if (client_result.error().response_code > 0) {
      error.response_code = client_result.error().response_code;
    }
    CompleteFetch(base::unexpected(std::move(error)));
    return;
  }

  std::optional<ProvisioningDomainProxyConfig> parsed_config =
      ParseProvisioningDomainConfig(*client_result);

  if (!parsed_config.has_value()) {
    CompleteFetch(base::unexpected(ProvisioningDomainFetchError(
        ProvisioningDomainFetchResultStatus::kParseError)));
    return;
  }

  CompleteFetch(std::move(*parsed_config));
}

void ProvisioningDomainFetcher::CompleteFetch(
    const ProvisioningDomainFetchResult& result) {
  if (!result.has_value()) {
    CHECK_NE(result.error().status,
             ProvisioningDomainFetchResultStatus::kSuccess)
        << "ProvisioningDomainFetchError status cannot be kSuccess";
  }

  ProvisioningDomainFetchResultStatus status =
      result.has_value() ? ProvisioningDomainFetchResultStatus::kSuccess
                         : result.error().status;

  base::UmaHistogramEnumeration("Enterprise.ProvisioningDomain.FetchResult",
                                status);

  std::vector<FetchCompleteCallback> callbacks = std::move(pending_callbacks_);
  for (auto& cb : callbacks) {
    std::move(cb).Run(result);
  }
}

}  // namespace enterprise_net
