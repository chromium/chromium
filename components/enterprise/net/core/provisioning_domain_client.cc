// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/provisioning_domain_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/enterprise/net/core/features.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace enterprise_net {

namespace {

// Network traffic annotation for Provisioning Domain configuration fetches.
const net::NetworkTrafficAnnotationTag kPvdTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("enterprise_proxy_pvd_fetch", R"(
        semantics {
          sender: "Enterprise Proxy Provisioning Domains Service"
          description:
            "Fetches dynamic proxy configuration and routing rules from a "
            "Provisioning Domain (PvD) server configured by the "
            "enterprise administrator."
          trigger:
            "When new network configurations are needed from Provisioning "
            "Domains, including but not limited to policy value, network "
            "change, signed in account change."
          data:
            "May include OAuth access token and custom headers (such as "
            "profile ID, preferred language) for Provisioning Domains."
          destination: OTHER
          destination_other:
            "The endpoint URL is constructed from the domains specified in the "
            "ProxyProvisioningDomains policy."
          internal {
            contacts {
              email: "xzonghan@google.com"
            }
            contacts {
              owners: "//components/enterprise/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: PROFILE_DATA
          }
          last_reviewed: "2026-07-06"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by users. It is only active when "
            "configured by an enterprise administrator via the "
            "ProxyProvisioningDomains policy."
          chrome_policy {
            ProxyProvisioningDomains {
              ProxyProvisioningDomains: "[]"
            }
          }
        })");

void RecordFetchResultMetrics(const ProvisioningDomainClientResult& result) {
  if (result.has_value()) {
    base::UmaHistogramEnumeration(
        "Enterprise.ProvisioningDomainClient.FetchResult",
        ProvisioningDomainClientFetchResult::kSuccess);
    return;
  }

  if (result.error().net_error != net::OK) {
    base::UmaHistogramEnumeration(
        "Enterprise.ProvisioningDomainClient.FetchResult",
        ProvisioningDomainClientFetchResult::kNetError);
    base::UmaHistogramSparse("Enterprise.ProvisioningDomainClient.NetError",
                             -result.error().net_error);
    return;
  }

  base::UmaHistogramEnumeration(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kHttpError);
  if (result.error().response_code != -1) {
    base::UmaHistogramSparse(
        "Enterprise.ProvisioningDomainClient.HttpResponseCode",
        result.error().response_code);
  }
}

}  // namespace

ProvisioningDomainClient::ProvisioningDomainClient(
    const GURL& url,
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_(url),
      delegate_(delegate),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(delegate_);
  CHECK(url_loader_factory_);
}

ProvisioningDomainClient::~ProvisioningDomainClient() = default;

void ProvisioningDomainClient::Fetch(
    const std::optional<std::string>& access_token,
    FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url_.is_valid()) {
    ProvisioningDomainClientResult result =
        base::unexpected(ProvisioningDomainClientError{
            .net_error = net::ERR_INVALID_URL, .response_code = -1});
    RecordFetchResultMetrics(result);
    std::move(callback).Run(std::move(result));
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  // If a request is already in-flight, queue the callback and reuse the fetch.
  if (is_fetching()) {
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;
  request->method = "GET";
  // Bypass proxy to avoid deadlock with network traffic pauses.
  request->load_flags = net::LOAD_BYPASS_PROXY;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  if (delegate_) {
    request->headers.MergeFrom(delegate_->GetExtraHeaders());
  }

  if (access_token.has_value() && !access_token->empty()) {
    request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                               base::StrCat({"Bearer ", *access_token}));
  }

  url_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                 kPvdTrafficAnnotation);

  url_loader_->SetRetryOptions(/*max_retries=*/2,
                               network::SimpleURLLoader::RETRY_ON_5XX);
  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ProvisioningDomainClient::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()),
      GetPvdConfigMaxSizeBytes());
}

void ProvisioningDomainClient::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_.reset();
  pending_callbacks_.clear();
}

void ProvisioningDomainClient::OnURLLoaderComplete(
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  int net_error = url_loader_->NetError();
  bool is_success = (net_error == net::OK) && response_body.has_value() &&
                    (response_code >= 200 && response_code <= 299);

  ProvisioningDomainClientResult result;
  if (is_success) {
    result = *response_body;
  } else {
    result = base::unexpected(ProvisioningDomainClientError{
        .net_error = net_error, .response_code = response_code});
  }

  RecordFetchResultMetrics(result);

  url_loader_.reset();

  // Move pending callbacks to a local vector before execution to prevent
  // iterator invalidation or re-entrancy issues if a callback invokes Fetch().
  std::vector<FetchCallback> callbacks = std::move(pending_callbacks_);
  pending_callbacks_.clear();
  for (auto& cb : callbacks) {
    std::move(cb).Run(result);
  }
}

}  // namespace enterprise_net
