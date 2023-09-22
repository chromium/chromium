// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "components/plus_addresses/features.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace plus_addresses {

namespace {

const base::TimeDelta kRequestTimeout = base::Seconds(5);

// See docs/network_traffic_annotations.md for reference.
// TODO(b/277532955): Update the description and trigger fields when possible.
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
        data: "The site on which the user wants to use a plus address is sent."
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

// TODO(b/277532955): Update the description and trigger fields when possible.
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

void PlusAddressClient::CreatePlusAddress(const std::string& site,
                                          PlusAddressCallback callback) {
  if (!server_url_) {
    return;
  }
  // Refresh the OAuth token if it's expired.
  if (access_token_info_.expiration_time < clock_->Now()) {
    GetAuthToken(base::BindOnce(&PlusAddressClient::CreatePlusAddress,
                                base::Unretained(this), site,
                                std::move(callback)));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPutMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerPlusProfileEndpoint);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_info_.token}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  base::Value::Dict payload;
  payload.Set("facet", site);
  std::string request_body;
  bool wrote_payload = base::JSONWriter::Write(payload, &request_body);
  DCHECK(wrote_payload);

  // TODO(b/300443275): Handle potential race to `loader_for_creation_` here.
  loader_for_creation_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kCreatePlusAddressAnnotation);
  loader_for_creation_->AttachStringForUpload(request_body, "application/json");
  loader_for_creation_->SetTimeoutDuration(kRequestTimeout);
  // Use max download size for now.
  // TODO(kaklilu) - Measure average downloadsize and pick a more appropriate
  // one.
  loader_for_creation_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](PlusAddressCallback callback,
             std::unique_ptr<std::string> response) {
            if (!response) {
              // The request has failed.
              // TODO: Add metrics here.
              return;
            }
            data_decoder::DataDecoder::ParseJsonIsolated(
                *response,
                base::BindOnce(&PlusAddressParser::ParsePlusAddressFromV1Create)
                    .Then(base::BindOnce(
                        [](PlusAddressCallback callback,
                           absl::optional<std::string> result) {
                          if (result.has_value()) {
                            std::move(callback).Run(result.value());
                          }
                        },
                        std::move(callback))));
          },
          std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void PlusAddressClient::GetAllPlusAddresses(PlusAddressMapCallback callback) {
  if (!server_url_) {
    return;
  }
  // Refresh the OAuth token if it's expired.
  if (access_token_info_.expiration_time < clock_->Now()) {
    GetAuthToken(base::BindOnce(&PlusAddressClient::GetAllPlusAddresses,
                                base::Unretained(this), std::move(callback)));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerPlusProfileEndpoint);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_info_.token}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // TODO(b/300443275): Handle potential race to `loader_for_retrieval_` here.
  loader_for_retrieval_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kGetAllPlusAddressesAnnotation);
  loader_for_retrieval_->SetTimeoutDuration(kRequestTimeout);
  // Use max download size for now.
  // TODO(kaklilu) - Measure average downloadsize and pick a more appropriate
  // one.
  loader_for_retrieval_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](PlusAddressMapCallback callback,
             std::unique_ptr<std::string> response) {
            if (!response) {
              // The request has failed.
              // TODO: Add metrics here.
              return;
            }

            data_decoder::DataDecoder::ParseJsonIsolated(
                *response,
                base::BindOnce(
                    &PlusAddressParser::ParsePlusAddressMapFromV1List)
                    .Then(base::BindOnce(
                        [](PlusAddressMapCallback callback,
                           absl::optional<PlusAddressMap> result) {
                          if (result.has_value()) {
                            std::move(callback).Run(result.value());
                          }
                        },
                        std::move(callback))));
          },
          std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

// TODO (kaklilu): Handle requests when token is nearing expiration.
void PlusAddressClient::GetAuthToken(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(access_token_info_.expiration_time < clock_->Now());
  // Enqueue `callback` to be run after the token is fetched.
  pending_callbacks_.emplace(std::move(callback));
  if (!access_token_fetcher_) {
    // Only request an auth token if it's not yet pending.
    RequestAuthToken();
  }
}

void PlusAddressClient::RequestAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"PlusAddressClient", identity_manager_, scopes_,
          base::BindOnce(&PlusAddressClient::OnTokenFetched,
                         // It is safe to use base::Unretained as
                         // |this| owns |access_token_fetcher_|.
                         base::Unretained(this)),
          // Use WaitUntilAvailable to defer getting an OAuth token until
          // the user is signed in. We can switch to kImmediate once we
          // have a sign in observer that guarantees we're already signed in
          // by this point.
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          // Sync doesn't need to be enabled for us to use PlusAddresses.
          signin::ConsentLevel::kSignin);
}

void PlusAddressClient::OnTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    access_token_info_ = access_token_info;
    // Run stored callbacks.
    while (!pending_callbacks_.empty()) {
      std::move(pending_callbacks_.front()).Run();
      pending_callbacks_.pop();
    }
  } else {
    access_token_request_error_ = error;
    // TODO (kaklilu): Replace this log with Histogram of OAuth errors.
    VLOG(1) << "PlusAddressClient failed to get OAuth token:"
            << error.ToString();
  }
}

}  // namespace plus_addresses
