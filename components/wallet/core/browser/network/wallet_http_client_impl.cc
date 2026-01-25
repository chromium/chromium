// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_http_client_impl.h"

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/data_models/walletable_pass.h"
#include "components/wallet/core/common/wallet_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {
namespace {
constexpr char kSavePassRequestPath[] = "v1/passes:upsert";
constexpr int kExternalIdNamespaceChrome = 1;

base::DictValue BuildExternalId() {
  base::DictValue external_id;
  external_id.Set("namespace", kExternalIdNamespaceChrome);
  external_id.Set("external_id",
                  base::Uuid::GenerateRandomV4().AsLowercaseString());
  return external_id;
}

base::DictValue BuildClientInfo() {
  base::DictValue chrome_client_info;
  chrome_client_info.Set("version", version_info::GetVersionNumber());

  base::DictValue client_info;
  client_info.Set("chrome_client_info", std::move(chrome_client_info));
  return client_info;
}

std::string BuildLoyaltyCardRequest(const LoyaltyCard& card) {
  // TODO(crbug.com/468916773): Migrate to protobuf to ensure type safety.
  base::DictValue request_dict;

  base::DictValue pass_dict;
  pass_dict.Set("external_id", BuildExternalId());

  base::DictValue loyalty_card_dict;
  loyalty_card_dict.Set("merchant_name", card.issuer_name);
  loyalty_card_dict.Set("loyalty_number", card.member_id);
  loyalty_card_dict.Set("program_name", card.plan_name);
  pass_dict.Set("loyalty_card", std::move(loyalty_card_dict));

  request_dict.Set("pass", std::move(pass_dict));
  request_dict.Set("client_info", BuildClientInfo());

  std::string json_output;
  base::JSONWriter::Write(request_dict, &json_output);
  return json_output;
}

std::string BuildEventPassRequest(const EventPass& pass) {
  // TODO(crbug.com/468916773): Implement EventPass request building.
  return std::string();
}

std::string BuildBoardingPassRequest(const BoardingPass& pass) {
  // TODO(crbug.com/468916773): Implement BoardingPass request building.
  return std::string();
}

std::string BuildTransitTicketRequest(const TransitTicket& ticket) {
  // TODO(crbug.com/468916773): Implement TransitTicket request building.
  return std::string();
}

std::string BuildSavePassRequest(const WalletablePass& pass) {
  return std::visit(
      absl::Overload{
          [](const LoyaltyCard& card) { return BuildLoyaltyCardRequest(card); },
          [](const EventPass& pass) { return BuildEventPassRequest(pass); },
          [](const BoardingPass& pass) {
            return BuildBoardingPassRequest(pass);
          },
          [](const TransitTicket& ticket) {
            return BuildTransitTicketRequest(ticket);
          }},
      pass.pass_data);
}
}  // namespace

WalletHttpClientImpl::WalletHttpClientImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(CHECK_DEREF(identity_manager)),
      url_loader_factory_(std::move(url_loader_factory)) {}

WalletHttpClientImpl::~WalletHttpClientImpl() = default;

void WalletHttpClientImpl::SavePass(const WalletablePass& pass,
                                    SavePassCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendRequest(
      kSavePassRequestPath, BuildSavePassRequest(pass),
      base::BindOnce(&WalletHttpClientImpl::OnSavePassResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WalletHttpClientImpl::SendRequest(
    const std::string& request_path,
    const std::string& request_body,
    base::OnceCallback<void(HttpResponse)> response_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetAuthToken(base::BindOnce(&WalletHttpClientImpl::SendRequestInternal,
                              weak_ptr_factory_.GetWeakPtr(), request_path,
                              request_body, std::move(response_callback)));
}

void WalletHttpClientImpl::GetAuthToken(TokenReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_token_callbacks_.push(std::move(callback));
  if (access_token_fetcher_) {
    return;
  }

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          // TODO(crbug.com/468916773): Replace with wallet auth id
          signin::OAuthConsumerId::kPaymentsAccessTokenFetcher,
          &identity_manager_.get(),
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

  std::optional<std::string> access_token;
  // TODO(crbug.com/471165306): Report error to UMA.
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
    const std::string& request_path,
    const std::string& request_body,
    base::OnceCallback<void(HttpResponse)> response_callback,
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!access_token) {
    std::move(response_callback)
        .Run(base::unexpected(
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));
    return;
  }

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  GURL base_url(kWalletablePassSaveUrl.Get());
  resource_request->url = base_url.Resolve(request_path);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      base::StrCat({"Bearer ", *access_token}));

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
  loader_ptr->AttachStringForUpload(request_body, "application/json");
  loader_ptr->SetAllowHttpErrorResults(true);
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&WalletHttpClientImpl::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), it,
                     std::move(response_callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void WalletHttpClientImpl::OnSimpleLoaderComplete(
    UrlLoaderList::iterator it,
    base::OnceCallback<void(HttpResponse)> response_callback,
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_loaders_.erase(it);
  if (!response_body) {
    // TODO(crbug.com/468915960): Handle detailed errors.
    std::move(response_callback)
        .Run(base::unexpected(
            WalletHttpClient::WalletRequestError::kGenericError));
    return;
  }
  std::move(response_callback).Run(std::move(*response_body));
}

void WalletHttpClientImpl::OnSavePassResponse(SavePassCallback callback,
                                              HttpResponse http_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!http_response.has_value()) {
    std::move(callback).Run(base::unexpected(http_response.error()));
    return;
  }

  // TODO(crbug.com/468916773): Parse the response body to extract pass_id.
  std::move(callback).Run(
      WalletHttpClient::SavePassResult{.pass_id = "dummy_pass_id"});
}

}  // namespace wallet
