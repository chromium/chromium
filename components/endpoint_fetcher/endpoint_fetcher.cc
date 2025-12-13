// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/endpoint_fetcher/endpoint_fetcher.h"

#include <optional>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace endpoint_fetcher {

namespace {
const char kContentTypeKey[] = "Content-Type";
const char kDeveloperKey[] = "X-Developer-Key";
const int kNumRetries = 3;
constexpr base::TimeDelta kDefaultTimeOut = base::Milliseconds(30000);

std::string GetHttpMethodString(const HttpMethod& http_method) {
  switch (http_method) {
    case HttpMethod::kGet:
      return "GET";
    case HttpMethod::kPost:
      return "POST";
    case HttpMethod::kDelete:
      return "DELETE";
    case HttpMethod::kPut:
      return "PUT";
    default:
      DCHECK(0) << base::StringPrintf("Unknown HttpMethod %d\n",
                                      static_cast<int>(http_method));
  }
  return "";
}

}  // namespace

EndpointResponse::EndpointResponse() = default;
EndpointResponse::EndpointResponse(const EndpointResponse& other) = default;
EndpointResponse& EndpointResponse::operator=(const EndpointResponse& other) =
    default;
EndpointResponse::~EndpointResponse() = default;

EndpointFetcher::RequestParams::RequestParams(
    const HttpMethod& method,
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : http_method_(method), annotation_tag_(annotation_tag) {}

EndpointFetcher::RequestParams::RequestParams(
    const EndpointFetcher::RequestParams& other) = default;
EndpointFetcher::RequestParams::~RequestParams() = default;

EndpointFetcher::RequestParams::Builder::Builder(
    const HttpMethod& method,
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : request_params_(
          std::make_unique<EndpointFetcher::RequestParams>(method,
                                                           annotation_tag)) {}

EndpointFetcher::RequestParams::Builder::Builder(
    const EndpointFetcher::RequestParams& other)
    : request_params_(std::make_unique<EndpointFetcher::RequestParams>(other)) {
}

EndpointFetcher::RequestParams::Builder::~Builder() = default;

EndpointFetcher::RequestParams
EndpointFetcher::RequestParams::Builder::Build() {
  // Perform consistency checks based on AuthType before building.
  switch (request_params_->auth_type_) {
    case OAUTH:
      DCHECK(request_params_->oauth_consumer_id.has_value())
          << "OAUTH requests require oauth_consumer_id.";
      DCHECK(request_params_->consent_level.has_value())
          << "OAUTH requests require consent_level.";
      break;
    case CHROME_API_KEY:
      DCHECK(request_params_->channel.has_value())
          << "CHROME_API_KEY requests require channel.";
      break;
    case NO_AUTH:
    default:
      break;
  }
  return *request_params_;
}

EndpointFetcher::EndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    signin::IdentityManager* identity_manager,
    RequestParams request_params)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      request_params_(std::move(request_params)) {
  if (request_params_.auth_type() == OAUTH) {
    DCHECK(identity_manager_)
        << "IdentityManager is required for OAUTH authentication.";
  }
}

// Protected constructor for mock objects (no specific dependencies are needed
// here).
EndpointFetcher::EndpointFetcher(
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : identity_manager_(nullptr),
      request_params_(
          EndpointFetcher::RequestParams::Builder(HttpMethod::kUndefined,
                                                  annotation_tag)
              .SetTimeout(kDefaultTimeOut)
              .Build()) {}

EndpointFetcher::~EndpointFetcher() = default;

void EndpointFetcher::Fetch(EndpointFetcherCallback endpoint_fetcher_callback) {
  DCHECK(!access_token_fetcher_);
  DCHECK(!simple_url_loader_);

  switch (request_params_.auth_type()) {
    case OAUTH: {
      // Should already be true due to constructor/builder DCHECKs, but serve as
      // final runtime sanity checks before dereferencing.
      DCHECK(identity_manager_);

      // Check if we have a primary account with the required consent level.
      if (!identity_manager_->HasPrimaryAccount(
              *request_params_.consent_level)) {
        auto response = std::make_unique<EndpointResponse>();
        VLOG(1) << __func__ << " No primary accounts found";
        response->response = "No primary accounts found";
        response->error_type =
            std::make_optional<FetchErrorType>(FetchErrorType::kAuthError);
        // TODO(crbug.com/40640190) Add more detailed error messaging
        std::move(endpoint_fetcher_callback).Run(std::move(response));
        return;
      }

      signin::AccessTokenFetcher::TokenCallback token_callback = base::BindOnce(
          &EndpointFetcher::OnAuthTokenFetched, weak_ptr_factory_.GetWeakPtr(),
          std::move(endpoint_fetcher_callback));

        access_token_fetcher_ = std::make_unique<
            signin::PrimaryAccountAccessTokenFetcher>(
            request_params_.oauth_consumer_id.value(), identity_manager_,
            std::move(token_callback),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
            *request_params_.consent_level);
      break;
    }
    case CHROME_API_KEY:
    case NO_AUTH:
    default: {
      // No asynchronous authentication needed; directly perform the HTTP
      // request.
      PerformHttpRequest(/*auth_token_key=*/nullptr,
                         std::move(endpoint_fetcher_callback));
      break;
    }
  }
}

void EndpointFetcher::PerformRequest(
    EndpointFetcherCallback endpoint_fetcher_callback,
    const char* key) {
  // TODO(crbug.com/284531303): Deprecate this method.
  PerformHttpRequest(key, std::move(endpoint_fetcher_callback));
}

void EndpointFetcher::OnAuthTokenFetched(
    EndpointFetcherCallback endpoint_fetcher_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    auto response = std::make_unique<EndpointResponse>();
    response->response = "There was an authentication error";
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kAuthError);
    // TODO(crbug.com/40640190) Add more detailed error messaging
    std::move(endpoint_fetcher_callback).Run(std::move(response));
    return;
  }
  // Proceed to perform the HTTP request using the fetched token.
  PerformHttpRequest(access_token_info.token.c_str(),
                     std::move(endpoint_fetcher_callback));
}

void EndpointFetcher::PerformHttpRequest(
    const char* auth_token_key,
    EndpointFetcherCallback endpoint_fetcher_callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = GetHttpMethodString(request_params_.http_method());
  resource_request->url = request_params_.url();
  resource_request->credentials_mode = GetCredentialsMode();

  if (GetSetSiteForCookies()) {
    resource_request->site_for_cookies =
        net::SiteForCookies::FromUrl(request_params_.url());
  }

  // Add Content-Type header if post data is present.
  bool has_body_content = (request_params_.http_method() == HttpMethod::kPost ||
                           request_params_.http_method() == HttpMethod::kPut) &&
                          request_params_.post_data();
  if (has_body_content) {
    resource_request->headers.SetHeader(kContentTypeKey,
                                        request_params_.content_type());
  }
  // Add custom headers.
  for (const auto& header : request_params_.headers()) {
    resource_request->headers.SetHeader(header.key, header.value);
  }
  // Add CORS-exempt headers.
  for (const auto& cors_exempt_header : request_params_.cors_exempt_headers()) {
    resource_request->cors_exempt_headers.SetHeaderIfMissing(
        cors_exempt_header.key, cors_exempt_header.value);
  }

  // Apply authentication headers based on AuthType.
  switch (request_params_.auth_type()) {
    case OAUTH:
      DCHECK(auth_token_key) << "OAuth token key is null for an OAUTH request.";
      resource_request->headers.SetHeader(
          kDeveloperKey, GaiaUrls::GetInstance()->oauth2_chrome_client_id());
      resource_request->headers.SetHeader(
          net::HttpRequestHeaders::kAuthorization,
          base::StringPrintf("Bearer %s", auth_token_key));
      break;
    case CHROME_API_KEY: {
      DCHECK(request_params_.channel)
          << "Channel is missing for CHROME_API_KEY request.";
      google_apis::AddDefaultAPIKeyToRequest(*resource_request,
                                             *request_params_.channel);
      break;
    }
    case NO_AUTH:
    default:
      break;
  }

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), request_params_.annotation_tag());

  if (has_body_content) {
    simple_url_loader_->AttachStringForUpload(
        request_params_.post_data().value(), request_params_.content_type());
  }
  if (auto upload_progress_callback = GetUploadProgressCallback();
      upload_progress_callback) {
    simple_url_loader_->SetOnUploadProgressCallback(upload_progress_callback);
  }

  simple_url_loader_->SetRetryOptions(GetMaxRetries(),
                                      network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->SetTimeoutDuration(request_params_.timeout());
  simple_url_loader_->SetAllowHttpErrorResults(true);

  network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback =
      base::BindOnce(&EndpointFetcher::OnResponseFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(endpoint_fetcher_callback));

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(), std::move(body_as_string_callback),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void EndpointFetcher::OnResponseFetched(
    EndpointFetcherCallback endpoint_fetcher_callback,
    std::optional<std::string> response_body) {
  auto response = std::make_unique<EndpointResponse>();
  std::string mime_type;
  if (const auto* response_info = simple_url_loader_->ResponseInfo();
      response_info && response_info->headers) {
    response->http_status_code = response_info->headers->response_code();
    mime_type = response_info->mime_type;
    response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        response_info->headers->raw_headers());
  }
  int net_error_code = simple_url_loader_->NetError();
  // The EndpointFetcher and its members will be destroyed after
  // any of the below callbacks. Do not access The EndpointFetcher
  // or its members after the callbacks.
  simple_url_loader_.reset();

  if (response->http_status_code == net::HTTP_UNAUTHORIZED ||
      response->http_status_code == net::HTTP_FORBIDDEN) {
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kAuthError);
    // We cannot assume that the response was in JSON, and hence cannot sanitize
    // the response. Send the respond as-is. For error cases, we may not have a
    // valid string pointer -- if we don't, send a simple message indicating
    // there was a response error (similar to below).
    // TODO: Think about how to better handle different MIME-types here.
    response->response = response_body ? std::move(response_body).value()
                                       : "There was a response error.";
    std::move(endpoint_fetcher_callback).Run(std::move(response));
    return;
  }

  if (net_error_code != net::OK) {
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kNetError);
  }

  if (response_body) {
    response->response = std::move(response_body).value();
  } else {
    std::string net_error = net::ErrorToString(net_error_code);
    VLOG(1) << __func__ << " with response error: " << net_error;
    response->response = "There was a response error";
  }
  std::move(endpoint_fetcher_callback).Run(std::move(response));
}

network::mojom::CredentialsMode EndpointFetcher::GetCredentialsMode() const {
  if (!request_params_.credentials_mode.has_value()) {
    return network::mojom::CredentialsMode::kOmit;
  }
  switch (request_params_.credentials_mode.value()) {
    case CredentialsMode::kOmit:
      return network::mojom::CredentialsMode::kOmit;
    case CredentialsMode::kInclude:
      return network::mojom::CredentialsMode::kInclude;
  }
  DCHECK(0) << base::StringPrintf(
      "Credentials mode %d not currently supported by EndpointFetcher\n",
      static_cast<int>(request_params_.credentials_mode.value()));
}

int EndpointFetcher::GetMaxRetries() const {
  return request_params_.max_retries.value_or(kNumRetries);
}

bool EndpointFetcher::GetSetSiteForCookies() const {
  return request_params_.set_site_for_cookies.value_or(false);
}

UploadProgressCallback EndpointFetcher::GetUploadProgressCallback() const {
  return request_params_.upload_progress_callback.value_or(
      UploadProgressCallback());
}

std::string EndpointFetcher::GetUrlForTesting() {
  return request_params_.url().spec();
}

}  // namespace endpoint_fetcher
