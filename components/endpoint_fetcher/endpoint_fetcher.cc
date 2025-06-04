// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/endpoint_fetcher/endpoint_fetcher.h"

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
    default:
      DCHECK(0) << base::StringPrintf("Unknown HttpMethod %d\n",
                                      static_cast<int>(http_method));
  }
  return "";
}

HttpMethod GetHttpMethod(const std::string& http_method_string) {
  if (http_method_string == "GET") {
    return HttpMethod::kGet;
  } else if (http_method_string == "POST") {
    return HttpMethod::kPost;
  } else if (http_method_string == "DELETE") {
    return HttpMethod::kDelete;
  }
  return HttpMethod::kUndefined;
}

}  // namespace

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
      DCHECK(request_params_->oauth_consumer_name.has_value() &&
             !request_params_->oauth_consumer_name->empty())
          << "OAUTH requests require oauth_consumer_name.";
      DCHECK(!request_params_->oauth_scopes.empty())
          << "OAUTH requests require oauth_scopes.";
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

EndpointFetcher::EndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const std::string& oauth_consumer_name,
    const GURL& url,
    const std::string& http_method,
    const std::string& content_type,
    const std::vector<std::string>& scopes,
    const base::TimeDelta& timeout,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    signin::IdentityManager* identity_manager,
    signin::ConsentLevel consent_level)
    : EndpointFetcher(oauth_consumer_name,
                      url,
                      http_method,
                      content_type,
                      scopes,
                      timeout,
                      post_data,
                      annotation_tag,
                      url_loader_factory,
                      identity_manager,
                      consent_level) {}

EndpointFetcher::EndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const GURL& url,
    const std::string& content_type,
    const base::TimeDelta& timeout,
    const std::string& post_data,
    const std::vector<std::string>& headers,
    const std::vector<std::string>& cors_exempt_headers,
    version_info::Channel channel,
    const RequestParams request_params)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(nullptr),
      request_params_(EndpointFetcher::RequestParams::Builder(request_params)
                          .SetAuthType(CHROME_API_KEY)
                          .SetUrl(url)
                          .SetContentType(content_type)
                          .SetTimeout(timeout)
                          .SetPostData(post_data)
                          .SetHeaders(headers)
                          .SetCorsExemptHeaders(cors_exempt_headers)
                          .SetChannel(channel)
                          .SetSanitizeResponse(true)
                          .Build()) {}

EndpointFetcher::EndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(nullptr),
      request_params_(EndpointFetcher::RequestParams::Builder(HttpMethod::kGet,
                                                              annotation_tag)
                          .SetAuthType(NO_AUTH)
                          .SetSanitizeResponse(false)
                          .SetTimeout(base::Milliseconds(0))
                          .SetUrl(url)
                          .Build()) {}

EndpointFetcher::EndpointFetcher(
    const std::string& oauth_consumer_name,
    const GURL& url,
    const std::string& http_method,
    const std::string& content_type,
    const std::vector<std::string>& scopes,
    const base::TimeDelta& timeout,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    signin::IdentityManager* identity_manager,
    signin::ConsentLevel consent_level)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      request_params_(
          EndpointFetcher::RequestParams::Builder(GetHttpMethod(http_method),
                                                  annotation_tag)
              .SetAuthType(OAUTH)
              .SetContentType(content_type)
              .SetTimeout(timeout)
              .SetPostData(post_data)
              .SetOauthConsumerName(oauth_consumer_name)
              .SetOauthScopes(scopes)
              .SetConsentLevel(consent_level)
              .SetSanitizeResponse(true)
              .SetUrl(url)
              .Build()) {}

// Protected constructor for mock objects (no specific dependencies are needed
// here).
EndpointFetcher::EndpointFetcher(
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : identity_manager_(nullptr),
      request_params_(
          EndpointFetcher::RequestParams::Builder(HttpMethod::kUndefined,
                                                  annotation_tag)
              .SetTimeout(kDefaultTimeOut)
              .SetSanitizeResponse(true)
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
          request_params_.oauth_consumer_name.value(), identity_manager_,
          request_params_.oauth_scopes, std::move(token_callback),
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
  if (request_params_.http_method() == HttpMethod::kPost &&
      request_params_.post_data()) {
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

  if (request_params_.http_method() == HttpMethod::kPost &&
      request_params_.post_data()) {
    simple_url_loader_->AttachStringForUpload(
        request_params_.post_data().value(), request_params_.content_type());
  }
  if (!GetUploadProgressCallback().is_null()) {
    simple_url_loader_->SetOnUploadProgressCallback(
        GetUploadProgressCallback());
  }

  simple_url_loader_->SetRetryOptions(GetMaxRetries(),
                                      network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->SetTimeoutDuration(request_params_.timeout());
  simple_url_loader_->SetAllowHttpErrorResults(true);

  network::SimpleURLLoader::BodyAsStringCallbackDeprecated
      body_as_string_callback = base::BindOnce(
          &EndpointFetcher::OnResponseFetched, weak_ptr_factory_.GetWeakPtr(),
          std::move(endpoint_fetcher_callback));

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(), std::move(body_as_string_callback),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void EndpointFetcher::OnResponseFetched(
    EndpointFetcherCallback endpoint_fetcher_callback,
    std::unique_ptr<std::string> response_body) {
  int http_status_code = -1;
  std::string mime_type;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    http_status_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
    mime_type = simple_url_loader_->ResponseInfo()->mime_type;
  }
  int net_error_code = simple_url_loader_->NetError();
  // The EndpointFetcher and its members will be destroyed after
  // any of the below callbacks. Do not access The EndpointFetcher
  // or its members after the callbacks.
  simple_url_loader_.reset();

  auto response = std::make_unique<EndpointResponse>();
  response->http_status_code = http_status_code;

  if (http_status_code == net::HTTP_UNAUTHORIZED ||
      http_status_code == net::HTTP_FORBIDDEN) {
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kAuthError);
    // We cannot assume that the response was in JSON, and hence cannot sanitize
    // the response. Send the respond as-is. For error cases, we may not have a
    // valid string pointer -- if we don't, send a simple message indicating
    // there was a response error (similar to below).
    // TODO: Think about how to better handle different MIME-types here.
    response->response =
        response_body ? *response_body : "There was a response error.";
    std::move(endpoint_fetcher_callback).Run(std::move(response));
    return;
  }

  if (net_error_code != net::OK) {
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kNetError);
  }

  if (response_body) {
    // Sanitize response if enabled and content type is JSON.
    // Use value_or(true) to default to sanitization if not explicitly set.
    if (request_params_.sanitize_response.value_or(true) &&
        mime_type == "application/json") {
      data_decoder::JsonSanitizer::Sanitize(
          std::move(*response_body),
          base::BindOnce(&EndpointFetcher::OnSanitizationResult,
                         weak_ptr_factory_.GetWeakPtr(), std::move(response),
                         std::move(endpoint_fetcher_callback)));
    } else {
      response->response = *response_body;
      std::move(endpoint_fetcher_callback).Run(std::move(response));
    }
  } else {
    std::string net_error = net::ErrorToString(net_error_code);
    VLOG(1) << __func__ << " with response error: " << net_error;
    response->response = "There was a response error";
    std::move(endpoint_fetcher_callback).Run(std::move(response));
  }
}

void EndpointFetcher::OnSanitizationResult(
    std::unique_ptr<EndpointResponse> response,
    EndpointFetcherCallback endpoint_fetcher_callback,
    data_decoder::JsonSanitizer::Result result) {
  if (result.has_value()) {
    response->response = result.value();
  } else {
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kResultParseError);
    response->response = "There was a sanitization error: " + result.error();
  }
  // The EndpointFetcher and its members will be destroyed after
  // any the below callback. Do not access The EndpointFetcher
  // or its members after the callback.
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
  if (!request_params_.max_retries.has_value()) {
    return kNumRetries;
  }
  return request_params_.max_retries.value();
}

bool EndpointFetcher::GetSetSiteForCookies() const {
  if (!request_params_.set_site_for_cookies.has_value()) {
    return false;
  }
  return request_params_.set_site_for_cookies.value();
}

UploadProgressCallback EndpointFetcher::GetUploadProgressCallback() const {
  if (!request_params_.upload_progress_callback.has_value()) {
    return UploadProgressCallback();
  }
  return request_params_.upload_progress_callback.value();
}

std::string EndpointFetcher::GetUrlForTesting() {
  return request_params_.url().spec();
}

}  // namespace endpoint_fetcher
