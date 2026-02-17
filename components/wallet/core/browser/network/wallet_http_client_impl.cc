// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_http_client_impl.h"

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "components/wallet/core/browser/metrics/wallet_metrics.h"
#include "components/wallet/core/browser/network/get_unmasked_pass_request.h"
#include "components/wallet/core/browser/network/upsert_private_pass_request.h"
#include "components/wallet/core/browser/network/upsert_public_pass_request.h"
#include "components/wallet/core/browser/network/wallet_request.h"
#include "components/wallet/core/common/wallet_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace wallet {
namespace {

// Determines whether a HTTP request was successful based on its response code.
bool IsHttpSuccess(int response_code) {
  return response_code >= 200 && response_code < 300;
}

}  // namespace

WalletHttpClientImpl::WalletHttpClientImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(CHECK_DEREF(identity_manager)),
      url_loader_factory_(std::move(url_loader_factory)) {}

WalletHttpClientImpl::~WalletHttpClientImpl() = default;

void WalletHttpClientImpl::UpsertPublicPass(Pass pass,
                                            UpsertPublicPassCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kWalletablePassDetection));
  SendRequest(std::make_unique<UpsertPublicPassRequest>(std::move(pass),
                                                        std::move(callback)));
}

void WalletHttpClientImpl::UpsertPrivatePass(
    PrivatePass pass,
    UpsertPrivatePassCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kWalletApiPrivatePassesEnabled));
  SendRequest(std::make_unique<UpsertPrivatePassRequest>(std::move(pass),
                                                         std::move(callback)));
}

void WalletHttpClientImpl::GetUnmaskedPass(std::string_view pass_id,
                                           GetUnmaskedPassCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendRequest(std::make_unique<GetUnmaskedPassRequest>(std::string(pass_id),
                                                       std::move(callback)));
}

void WalletHttpClientImpl::SendRequest(std::unique_ptr<WalletRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetAuthToken(base::BindOnce(&WalletHttpClientImpl::SendRequestInternal,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(request)));
}

void WalletHttpClientImpl::GetAuthToken(TokenReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_token_callbacks_.push(std::move(callback));
  if (access_token_fetcher_) {
    return;
  }

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kWalletPasses, &identity_manager_.get(),
          base::BindOnce(&WalletHttpClientImpl::OnTokenFetched,
                         weak_ptr_factory_.GetWeakPtr()),
          // The user must be signed in to make requests.
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

void WalletHttpClientImpl::OnTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  metrics::RecordNetworkRequestOauthError(error);

  std::optional<std::string> access_token;
  if (error.state() == GoogleServiceAuthError::NONE) {
    access_token = access_token_info.token;
  }

  // Use a local 'snapshot' to protect against re-entrancy.
  base::queue<TokenReadyCallback> callbacks;
  std::swap(pending_token_callbacks_, callbacks);

  while (!callbacks.empty()) {
    std::move(callbacks.front()).Run(access_token);
    callbacks.pop();
  }
}

void WalletHttpClientImpl::SendRequestInternal(
    std::unique_ptr<WalletRequest> request,
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!access_token) {
    std::move(*request).OnResponse(base::unexpected(
        WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));
    return;
  }

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL base_url(features::kWalletSaveUrl.Get());
  resource_request->url = base_url.Resolve(request->GetRequestUrlPath());
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      base::StrCat({"Bearer ", *access_token}));
  resource_request->headers.MergeFrom(request->GetRequestHeaders());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("wallet_http_request", R"(
        semantics {
          sender: "Wallet Http Client"
          description:
            "Sends a generic request to Google Wallet backend."
          trigger:
            "User triggers a wallet action."
          data:
            "JSON data specific to the request."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "wallet-1p-integrations@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
          }
          last_reviewed: "2025-12-16"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this feature by turning off"
            " 'Save passes to Wallet' in settings."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
            }
          }
        })");

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);

  network::SimpleURLLoader* loader_ptr = simple_url_loader.get();
  UrlLoaderList::iterator it = active_loaders_.insert(
      active_loaders_.begin(), std::move(simple_url_loader));
  loader_ptr->AttachStringForUpload(request->GetRequestContent(),
                                    "application/protobuf");
  loader_ptr->SetAllowHttpErrorResults(true);
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&WalletHttpClientImpl::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), it, std::move(request),
                     base::TimeTicks::Now()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void WalletHttpClientImpl::OnSimpleLoaderComplete(
    UrlLoaderList::iterator it,
    std::unique_ptr<WalletRequest> request,
    base::TimeTicks request_start,
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<network::SimpleURLLoader> loader = std::move(*it);
  active_loaders_.erase(it);
  int http_response_code = -1;  // Invalid response code.
  if (loader) {
    if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
      http_response_code = loader->ResponseInfo()->headers->response_code();
    }
    int net_error = loader->NetError();
    // Log the HTTP response or error code and request duration.
    metrics::RecordHttpResponseOrErrorCode(
        request->GetRequestType(),
        net_error != net::OK && net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE
            ? net_error
            : http_response_code);

    metrics::RecordNetworkRequestLatency(
        request->GetRequestType(), base::TimeTicks::Now() - request_start);
  }

  if (response_body) {
    metrics::RecordNetworkRequestResponseSize(request->GetRequestType(),
                                              response_body->size());
  }

  const bool success = response_body && IsHttpSuccess(http_response_code);
  if (!success) {
    std::move(*request).OnResponse(
        base::unexpected(WalletHttpClient::WalletRequestError::kGenericError));
    return;
  }
  std::move(*request).OnResponse(std::move(*response_body));
}

}  // namespace wallet
