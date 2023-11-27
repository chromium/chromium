// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_parser.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace plus_addresses {

namespace {

const base::TimeDelta kRequestTimeout = base::Seconds(5);

// See docs/network_traffic_annotations.md for reference.
// TODO(b/295556954): Update the description and trigger fields when possible.
//                    Also replace the policy_exception when we have a policy.
const net::NetworkTrafficAnnotationTag kCreatePlusAddressAnnotation =
    net::DefineNetworkTrafficAnnotation("plus_address_creation", R"(
      semantics {
        sender: "Chrome Plus Address Client"
        description: "A plus address is created on the enterprise-specified "
                      "server with this request."
        trigger: "User chooses to create a plus address."
        internal {
          contacts {
              email: "dc-komics@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN,
          type: SENSITIVE_URL
        }
        data: "The origin on which the user wants to use a plus address is "
                "sent."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-09-07"
      }
      policy {
        cookies_allowed: NO
        setting: "Disable the Plus Addresses feature."
        policy_exception_justification: "We don't have an opt-out policy yet"
                                        " as Plus Addresses hasn't launched."
      }
    )");

// TODO(b/295556954): Update the description and trigger fields when possible.
//                    Also replace the policy_exception when we have a policy.
const net::NetworkTrafficAnnotationTag kReservePlusAddressAnnotation =
    net::DefineNetworkTrafficAnnotation("plus_address_reservation", R"(
      semantics {
        sender: "Chrome Plus Address Client"
        description: "A plus address is reserved for the user on the "
                      "enterprise-specified server with this request."
        trigger: "User enters the create plus address UX flow."
        internal {
          contacts {
              email: "dc-komics@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN,
          type: SENSITIVE_URL
        }
        data: "The origin that the user may use a plus address on is sent."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-09-23"
      }
      policy {
        cookies_allowed: NO
        setting: "Disable the Plus Addresses feature."
        policy_exception_justification: "We don't have an opt-out policy yet"
                                        " as Plus Addresses hasn't launched."
      }
    )");

// TODO(b/277532955): Update the description and trigger fields when possible.
//                    Also replace the policy_exception when we have a policy.
const net::NetworkTrafficAnnotationTag kConfirmPlusAddressAnnotation =
    net::DefineNetworkTrafficAnnotation("plus_address_confirmation", R"(
      semantics {
        sender: "Chrome Plus Address Client"
        description: "A plus address is confirmed for creation on the "
                      "enterprise-specified server with this request."
        trigger: "User confirms to create the displayed plus address."
        internal {
          contacts {
              email: "dc-komics@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN,
          type: SENSITIVE_URL,
          type: USERNAME
        }
        data: "The plus address and the origin that the user is using it on "
              "are  both sent."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-09-23"
      }
      policy {
        cookies_allowed: NO
        setting: "Disable the Plus Addresses feature."
        policy_exception_justification: "We don't have an opt-out policy yet"
                                        " as Plus Addresses hasn't launched."
      }
    )");

// TODO(b/295556954): Update the description and trigger fields when possible.
//                    Also replace the policy_exception when we have a policy.
const net::NetworkTrafficAnnotationTag kGetAllPlusAddressesAnnotation =
    net::DefineNetworkTrafficAnnotation("get_all_plus_addresses", R"(
      semantics {
        sender: "Chrome Plus Address Client"
        description: "This request fetches all plus addresses from the "
                      "enterprise-specified server."
        trigger: "n/a. This happens in the background to keep the PlusAddress "
                 "service in sync with the remote server."
        internal {
          contacts {
              email: "dc-komics@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        data: "n/a"
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-09-13"
      }
      policy {
        cookies_allowed: NO
        setting: "Disable the Plus Addresses feature."
        policy_exception_justification: "We don't have an opt-out policy yet"
                                        " as Plus Addresses hasn't launched."
      }
    )");

absl::optional<GURL> ValidateAndGetUrl() {
  GURL maybe_url = GURL(kEnterprisePlusAddressServerUrl.Get());
  return maybe_url.is_valid() ? absl::make_optional(maybe_url) : absl::nullopt;
}

}  // namespace

PlusAddressClient::PlusAddressClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      server_url_(ValidateAndGetUrl()),
      scopes_({kEnterprisePlusAddressOAuthScope.Get()}) {}

PlusAddressClient::~PlusAddressClient() = default;
PlusAddressClient::PlusAddressClient(PlusAddressClient&&) = default;
PlusAddressClient& PlusAddressClient::operator=(PlusAddressClient&&) = default;

void PlusAddressClient::CreatePlusAddress(const url::Origin& origin,
                                          PlusAddressCallback callback) {
  if (!server_url_) {
    return;
  }
  GetAuthToken(base::BindOnce(&PlusAddressClient::CreatePlusAddressInternal,
                              base::Unretained(this), origin,
                              std::move(callback)));
}

void PlusAddressClient::ReservePlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (!server_url_) {
    return;
  }
  GetAuthToken(base::BindOnce(&PlusAddressClient::ReservePlusAddressInternal,
                              base::Unretained(this), origin,
                              std::move(on_completed)));
}

void PlusAddressClient::ConfirmPlusAddress(
    const url::Origin& origin,
    const std::string& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (!server_url_) {
    return;
  }
  GetAuthToken(base::BindOnce(&PlusAddressClient::ConfirmPlusAddressInternal,
                              base::Unretained(this), origin, plus_address,
                              std::move(on_completed)));
}

void PlusAddressClient::GetAllPlusAddresses(PlusAddressMapCallback callback) {
  if (!server_url_) {
    return;
  }
  GetAuthToken(base::BindOnce(&PlusAddressClient::GetAllPlusAddressesInternal,
                              base::Unretained(this), std::move(callback)));
}

void PlusAddressClient::CreatePlusAddressInternal(
    const url::Origin& origin,
    PlusAddressCallback callback,
    absl::optional<std::string> auth_token) {
  if (!auth_token.has_value()) {
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPutMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerPlusProfileEndpoint);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", auth_token.value()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  base::Value::Dict payload;
  payload.Set("facet", origin.Serialize());
  std::string request_body;
  bool wrote_payload = base::JSONWriter::Write(payload, &request_body);
  DCHECK(wrote_payload);

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kCreatePlusAddressAnnotation);
  network::SimpleURLLoader* loader_ptr = loader.get();
  loader_ptr->AttachStringForUpload(request_body, "application/json");
  loader_ptr->SetTimeoutDuration(kRequestTimeout);
  // TODO(b/301984623) - Measure average downloadsize and change this.
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PlusAddressClient::OnCreatePlusAddressComplete,
                     // Safe since this class owns the list of loaders.
                     base::Unretained(this),
                     loaders_for_creation_.insert(loaders_for_creation_.begin(),
                                                  std::move(loader)),
                     clock_->Now(), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void PlusAddressClient::ReservePlusAddressInternal(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed,
    absl::optional<std::string> auth_token) {
  if (!auth_token.has_value()) {
    std::move(on_completed)
        .Run(base::unexpected(
            PlusAddressRequestError(PlusAddressRequestErrorType::kOAuthError)));
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPutMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerReservePlusAddressEndpoint);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", auth_token.value()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  base::Value::Dict payload;
  payload.Set("facet", origin.Serialize());
  std::string request_body;
  bool wrote_payload = base::JSONWriter::Write(payload, &request_body);
  DCHECK(wrote_payload);

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kReservePlusAddressAnnotation);
  network::SimpleURLLoader* loader_ptr = loader.get();
  loader_ptr->AttachStringForUpload(request_body, "application/json");
  loader_ptr->SetTimeoutDuration(kRequestTimeout);
  // TODO(b/301984623) - Measure average downloadsize and change this.
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PlusAddressClient::OnReserveOrConfirmPlusAddressComplete,
                     // Safe since this class owns the list of loaders.
                     base::Unretained(this),
                     loaders_for_creation_.insert(loaders_for_creation_.begin(),
                                                  std::move(loader)),
                     PlusAddressNetworkRequestType::kReserve, clock_->Now(),
                     std::move(on_completed)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void PlusAddressClient::ConfirmPlusAddressInternal(
    const url::Origin& origin,
    const std::string& plus_address,
    PlusAddressRequestCallback on_completed,
    absl::optional<std::string> auth_token) {
  if (!auth_token.has_value()) {
    std::move(on_completed)
        .Run(base::unexpected(
            PlusAddressRequestError(PlusAddressRequestErrorType::kOAuthError)));
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPutMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerCreatePlusAddressEndpoint);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", auth_token.value()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  base::Value::Dict payload;
  payload.Set("facet", origin.Serialize());
  payload.Set("reserved_email_address", plus_address);
  std::string request_body;
  bool wrote_payload = base::JSONWriter::Write(payload, &request_body);
  DCHECK(wrote_payload);

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kConfirmPlusAddressAnnotation);
  network::SimpleURLLoader* loader_ptr = loader.get();
  loader_ptr->AttachStringForUpload(request_body, "application/json");
  loader_ptr->SetTimeoutDuration(kRequestTimeout);
  // TODO(b/301984623) - Measure average downloadsize and change this.
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PlusAddressClient::OnReserveOrConfirmPlusAddressComplete,
                     // Safe since this class owns the list of loaders.
                     base::Unretained(this),
                     loaders_for_creation_.insert(loaders_for_creation_.begin(),
                                                  std::move(loader)),
                     PlusAddressNetworkRequestType::kCreate, clock_->Now(),
                     std::move(on_completed)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void PlusAddressClient::GetAllPlusAddressesInternal(
    PlusAddressMapCallback callback,
    absl::optional<std::string> auth_token) {
  if (!auth_token.has_value()) {
    return;
  }
  // Fail early if the URL Loader is already in-use. We never expect this method
  // to be called in quick succession.
  if (loader_for_sync_) {
    DCHECK(false);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerPlusProfileEndpoint);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", auth_token.value()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  loader_for_sync_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kGetAllPlusAddressesAnnotation);
  loader_for_sync_->SetTimeoutDuration(kRequestTimeout);
  loader_for_sync_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PlusAddressClient::OnGetAllPlusAddressesComplete,
                     // Safe since this class owns the loader_for_sync_.
                     base::Unretained(this), clock_->Now(),
                     std::move(callback)),
      // TODO(b/301984623) - Measure average downloadsize and change this.
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void PlusAddressClient::OnCreatePlusAddressComplete(
    UrlLoaderList::iterator it,
    base::Time request_start,
    PlusAddressCallback callback,
    std::unique_ptr<std::string> response) {
  // Record relevant metrics.
  std::unique_ptr<network::SimpleURLLoader> loader = std::move(*it);
  PlusAddressMetrics::RecordNetworkRequestLatency(
      PlusAddressNetworkRequestType::kGetOrCreate,
      clock_->Now() - request_start);
  if (loader && loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    PlusAddressMetrics::RecordNetworkRequestResponseCode(
        PlusAddressNetworkRequestType::kGetOrCreate,
        loader->ResponseInfo()->headers->response_code());
  }
  // Destroy the loader before returning.
  loaders_for_creation_.erase(it);
  if (!response) {
    return;
  }
  PlusAddressMetrics::RecordNetworkRequestResponseSize(
      PlusAddressNetworkRequestType::kGetOrCreate, response->size());
  // Parse the response & return it via callback.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&PlusAddressParser::ParsePlusProfileFromV1Create)
          .Then(base::BindOnce(
              [](PlusAddressCallback callback,
                 absl::optional<PlusProfile> result) {
                if (result.has_value()) {
                  std::move(callback).Run(result->plus_address);
                }
              },
              std::move(callback))));
}

void PlusAddressClient::OnReserveOrConfirmPlusAddressComplete(
    UrlLoaderList::iterator it,
    PlusAddressNetworkRequestType type,
    base::Time request_start,
    PlusAddressRequestCallback on_completed,
    std::unique_ptr<std::string> response) {
  // Record relevant metrics.
  std::unique_ptr<network::SimpleURLLoader> loader = std::move(*it);
  PlusAddressMetrics::RecordNetworkRequestLatency(
      type, clock_->Now() - request_start);
  if (loader && loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    PlusAddressMetrics::RecordNetworkRequestResponseCode(
        type, loader->ResponseInfo()->headers->response_code());
  }
  // Destroy the loader before returning.
  loaders_for_creation_.erase(it);
  if (!response) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kNetworkError)));
    return;
  }
  PlusAddressMetrics::RecordNetworkRequestResponseSize(type, response->size());
  // Parse the response & return it via callback.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&PlusAddressParser::ParsePlusProfileFromV1Create)
          .Then(base::BindOnce(
              [](PlusAddressRequestCallback callback,
                 absl::optional<PlusProfile> result) {
                if (!result.has_value()) {
                  std::move(callback).Run(
                      base::unexpected(PlusAddressRequestError(
                          PlusAddressRequestErrorType::kParsingError)));
                  return;
                }
                std::move(callback).Run(result.value());
              },
              std::move(on_completed))));
}

void PlusAddressClient::OnGetAllPlusAddressesComplete(
    base::Time request_start,
    PlusAddressMapCallback callback,
    std::unique_ptr<std::string> response) {
  // Record relevant metrics.
  PlusAddressMetrics::RecordNetworkRequestLatency(
      PlusAddressNetworkRequestType::kList, clock_->Now() - request_start);
  if (loader_for_sync_ && loader_for_sync_->ResponseInfo() &&
      loader_for_sync_->ResponseInfo()->headers) {
    PlusAddressMetrics::RecordNetworkRequestResponseCode(
        PlusAddressNetworkRequestType::kList,
        loader_for_sync_->ResponseInfo()->headers->response_code());
  }
  // Destroy the loader before returning.
  loader_for_sync_.reset();
  if (!response) {
    return;
  }
  PlusAddressMetrics::RecordNetworkRequestResponseSize(
      PlusAddressNetworkRequestType::kList, response->size());
  // Parse the response & return it via callback.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&PlusAddressParser::ParsePlusAddressMapFromV1List)
          .Then(base::BindOnce(
              [](PlusAddressMapCallback callback,
                 absl::optional<PlusAddressMap> result) {
                if (result.has_value()) {
                  std::move(callback).Run(result.value());
                }
              },
              std::move(callback))));
}

void PlusAddressClient::GetAuthToken(TokenReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (access_token_fetcher_) {
    pending_callbacks_.emplace(std::move(callback));
    return;
  }
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"PlusAddressClient", identity_manager_, scopes_,
          base::BindOnce(&PlusAddressClient::OnTokenFetched,
                         // It is safe to use base::Unretained as
                         // |this| owns |access_token_fetcher_|.
                         base::Unretained(this), std::move(callback)),
          // Use WaitUntilAvailable to defer getting an OAuth token until
          // the user is signed in. We can switch to kImmediate once we
          // have a sign in observer that guarantees we're already signed in
          // by this point.
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          // Sync doesn't need to be enabled for us to use PlusAddresses.
          signin::ConsentLevel::kSignin);
}

void PlusAddressClient::OnTokenFetched(
    TokenReadyCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  PlusAddressMetrics::RecordNetworkRequestOauthError(error);
  absl::optional<std::string> access_token;
  if (error.state() == GoogleServiceAuthError::NONE) {
    access_token = access_token_info.token;
  }
  std::move(callback).Run(access_token);
  while (!pending_callbacks_.empty()) {
    std::move(pending_callbacks_.front()).Run(access_token);
    pending_callbacks_.pop();
  }
}

}  // namespace plus_addresses
