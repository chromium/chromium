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
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
const char kContentTypeKey[] = "Content-Type";
const char kDeveloperKey[] = "X-Developer-Key";
const int kNumRetries = 3;
constexpr base::TimeDelta kDefaultTimeOut = base::Milliseconds(30000);
}  // namespace

EndpointFetcher::RequestParams::Builder::Builder()
    : request_params_(std::make_unique<RequestParams>()) {}

EndpointFetcher::RequestParams::Builder::~Builder() = default;

EndpointFetcher::RequestParams
EndpointFetcher::RequestParams::Builder::Build() {
  return *request_params_;
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
    const std::string& http_method,
    const std::string& content_type,
    const base::TimeDelta& timeout,
    const std::string& post_data,
    const std::vector<std::string>& headers,
    const std::vector<std::string>& cors_exempt_headers,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    version_info::Channel channel,
    const std::optional<RequestParams> request_params)
    : auth_type_(CHROME_API_KEY),
      url_(url),
      http_method_(http_method),
      content_type_(content_type),
      timeout_(timeout),
      post_data_(post_data),
      headers_(headers),
      cors_exempt_headers_(cors_exempt_headers),
      annotation_tag_(annotation_tag),
      url_loader_factory_(url_loader_factory),
      identity_manager_(nullptr),
      consent_level_(std::nullopt),
      sanitize_response_(true),
      channel_(channel),
      request_params_(request_params) {}

EndpointFetcher::EndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : auth_type_(NO_AUTH),
      url_(url),
      http_method_("GET"),
      content_type_(std::string()),
      timeout_(base::Milliseconds(0)),
      post_data_(std::string()),
      annotation_tag_(annotation_tag),
      url_loader_factory_(url_loader_factory),
      identity_manager_(nullptr),
      consent_level_(std::nullopt),
      sanitize_response_(false) {}

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
    : auth_type_(OAUTH),
      oauth_consumer_name_(oauth_consumer_name),
      url_(url),
      http_method_(http_method),
      content_type_(content_type),
      timeout_(timeout),
      post_data_(post_data),
      annotation_tag_(annotation_tag),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      consent_level_(consent_level),
      sanitize_response_(true) {
  for (auto scope : scopes) {
    oauth_scopes_.insert(scope);
  }
}

EndpointFetcher::EndpointFetcher(
    const GURL& url,
    const std::string& http_method,
    const std::string& content_type,
    const base::TimeDelta& timeout,
    const std::string& post_data,
    const std::vector<std::string>& headers,
    const std::vector<std::string>& cors_exempt_headers,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    bool is_oauth_fetch)
    : auth_type_(is_oauth_fetch ? OAUTH : CHROME_API_KEY),
      url_(url),
      http_method_(http_method),
      content_type_(content_type),
      timeout_(timeout),
      post_data_(post_data),
      headers_(headers),
      cors_exempt_headers_(cors_exempt_headers),
      annotation_tag_(annotation_tag),
      url_loader_factory_(url_loader_factory),
      identity_manager_(nullptr),
      consent_level_(std::nullopt),
      sanitize_response_(true) {}

EndpointFetcher::EndpointFetcher(
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : timeout_(kDefaultTimeOut),
      annotation_tag_(annotation_tag),
      identity_manager_(nullptr),
      consent_level_(std::nullopt),
      sanitize_response_(true) {}

EndpointFetcher::~EndpointFetcher() = default;

void EndpointFetcher::Fetch(EndpointFetcherCallback endpoint_fetcher_callback) {
  DCHECK(!access_token_fetcher_);
  DCHECK(!simple_url_loader_);
  DCHECK(identity_manager_);
  DCHECK(consent_level_);
  // Check if we have a primary account with the consent level provided to the
  // constructor.
  if (!identity_manager_->HasPrimaryAccount(*consent_level_)) {
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
  // TODO(crbug.com/40641804) Make access_token_fetcher_ local variable passed
  // to callback
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          oauth_consumer_name_, identity_manager_, oauth_scopes_,
          std::move(token_callback),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          *consent_level_);
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
  PerformRequest(std::move(endpoint_fetcher_callback),
                 access_token_info.token.c_str());
}

void EndpointFetcher::PerformRequest(
    EndpointFetcherCallback endpoint_fetcher_callback,
    const char* key) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = http_method_;
  resource_request->url = url_;
  resource_request->credentials_mode = GetCredentialsMode();

  if (base::EqualsCaseInsensitiveASCII(http_method_, "POST")) {
    resource_request->headers.SetHeader(kContentTypeKey, content_type_);
  }
  DCHECK_EQ(headers_.size() % 2, 0UL);
  for (size_t i = 0; i + 1 < headers_.size(); i += 2) {
    resource_request->headers.SetHeader(headers_[i], headers_[i + 1]);
  }
  DCHECK_EQ(cors_exempt_headers_.size() % 2, 0UL);
  for (size_t i = 0; i + 1 < cors_exempt_headers_.size(); i += 2) {
    resource_request->cors_exempt_headers.SetHeaderIfMissing(
        cors_exempt_headers_[i], cors_exempt_headers_[i + 1]);
  }
  switch (auth_type_) {
    case OAUTH:
      resource_request->headers.SetHeader(
          kDeveloperKey, GaiaUrls::GetInstance()->oauth2_chrome_client_id());
      resource_request->headers.SetHeader(
          net::HttpRequestHeaders::kAuthorization,
          base::StringPrintf("Bearer %s", key));
      break;
    case CHROME_API_KEY: {
      google_apis::AddDefaultAPIKeyToRequest(*resource_request, channel_);
      break;
    }
    default:
      break;
  }
  // TODO(crbug.com/40641804) Make simple_url_loader_ local variable passed to
  // callback
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), annotation_tag_);

  if (base::EqualsCaseInsensitiveASCII(http_method_, "POST")) {
    simple_url_loader_->AttachStringForUpload(post_data_, content_type_);
  }
  simple_url_loader_->SetRetryOptions(GetMaxRetries(),
                                      network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->SetTimeoutDuration(timeout_);
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
        response_body.get() ? *response_body : "There was a response error";
    std::move(endpoint_fetcher_callback).Run(std::move(response));
    return;
  }

  if (net_error_code != net::OK) {
    response->error_type =
        std::make_optional<FetchErrorType>(FetchErrorType::kNetError);
  }

  if (response_body) {
    if (sanitize_response_ && mime_type == "application/json") {
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

network::mojom::CredentialsMode EndpointFetcher::GetCredentialsMode() {
  if (!request_params_.has_value()) {
    return network::mojom::CredentialsMode::kOmit;
  }
  if (!request_params_.value().credentials_mode.has_value()) {
    return network::mojom::CredentialsMode::kOmit;
  }
  switch (request_params_.value().credentials_mode.value()) {
    case CredentialsMode::kOmit:
      return network::mojom::CredentialsMode::kOmit;
    case CredentialsMode::kInclude:
      return network::mojom::CredentialsMode::kInclude;
  }
  DCHECK(0) << base::StringPrintf(
      "Credentials mode %d not currently supported by EndpointFetcher\n",
      static_cast<int>(request_params_.value().credentials_mode.value()));
}

int EndpointFetcher::GetMaxRetries() {
  if (!request_params_.has_value()) {
    return kNumRetries;
  }
  if (!request_params_.value().max_retries.has_value()) {
    return kNumRetries;
  }
  return request_params_.value().max_retries.value();
}

std::string EndpointFetcher::GetUrlForTesting() {
  return url_.spec();
}
