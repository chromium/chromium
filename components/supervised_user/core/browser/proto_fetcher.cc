// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {
// Controls the retry count of the simple url loader.
const int kUrlLoaderRetryCount = 1;

bool IsLoadingSuccessful(const network::SimpleURLLoader& loader) {
  return loader.NetError() == net::OK;
}

bool HasHttpAuthErrorResponse(const network::SimpleURLLoader& loader) {
  if (!(loader.ResponseInfo() && loader.ResponseInfo()->headers)) {
    return false;
  }
  return net::HttpStatusCode(loader.ResponseInfo()->headers->response_code()) ==
         net::HTTP_UNAUTHORIZED;
}

bool HasHttpOkResponse(const network::SimpleURLLoader& loader) {
  if (!(loader.ResponseInfo() && loader.ResponseInfo()->headers)) {
    return false;
  }
  return net::HttpStatusCode(loader.ResponseInfo()->headers->response_code()) ==
         net::HTTP_OK;
}

// Return HTTP status if available, or net::Error otherwise. HTTP status takes
// precedence to avoid masking it by net::ERR_HTTP_RESPONSE_CODE_FAILURE.
// Returned value is positive for HTTP status and negative for net::Error,
// consistent with
// tools/metrics/histograms/enums.xml://enum[@name='CombinedHttpResponseAndNetErrorCode']
int HttpStatusOrNetError(const network::SimpleURLLoader& loader) {
  if (loader.ResponseInfo() && loader.ResponseInfo()->headers) {
    return loader.ResponseInfo()->headers->response_code();
  }
  return loader.NetError();
}

std::string CreateAuthorizationHeader(
    const signin::AccessTokenInfo& access_token_info) {
  // Do not use std::string_view with StringPrintf, see crbug/1444165
  return base::StrCat({kAuthorizationHeader, " ", access_token_info.token});
}

// Determines the response type. See go/system-parameters to see list of
// possible One Platform system params.
constexpr std::string_view kSystemParameters("alt=proto");

// Creates a request url for kids management api which is independent from the
// current profile (doesn't take Profile* parameter). It also adds query
// parameter that configures the remote endpoint to respond with a protocol
// buffer message.
GURL CreateRequestUrl(const FetcherConfig& config,
                      const FetcherConfig::PathArgs& args,
                      std::string_view query_string) {
  CHECK(!config.service_endpoint.Get().empty())
      << "Service endpoint is required";
  // kSystemParameters is unconditionally concatenated with the path. If it can
  // be empty, handle it in the code below.
  CHECK(!kSystemParameters.empty());

  std::string path_with_query = base::StrCat(
      {config.ServicePath(args), "?", std::string(kSystemParameters)});
  if (!query_string.empty()) {
    path_with_query += base::StrCat({"&", std::string(query_string)});
  }
  return GURL(config.service_endpoint.Get()).Resolve(path_with_query);
}

std::unique_ptr<network::SimpleURLLoader> InitializeSimpleUrlLoader(
    const std::optional<signin::AccessTokenInfo> access_token_info,
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args,
    std::optional<version_info::Channel> channel,
    const FetchProcess::Payload& payload) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url =
      CreateRequestUrl(fetcher_config, args, payload.query_string);
  resource_request->method = fetcher_config.GetHttpMethod();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->priority = fetcher_config.request_priority;

  if (access_token_info) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        CreateAuthorizationHeader(access_token_info.value()));
  } else {
    CHECK(channel);
    google_apis::AddDefaultAPIKeyToRequest(*resource_request, *channel);
  }

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       fetcher_config.traffic_annotation());

  if (fetcher_config.method != FetcherConfig::Method::kGet) {
    // Attach request body, even if it's empty, to all requests except for
    // GET.
    simple_url_loader->AttachStringForUpload(payload.request_body,
                                             "application/x-protobuf");
  }

  simple_url_loader->SetRetryOptions(
      kUrlLoaderRetryCount, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  return simple_url_loader;
}

}  // namespace

FetchProcess::FetchProcess(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const Payload& payload,
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args,
    std::optional<version_info::Channel> channel)
    : identity_manager_(identity_manager),
      payload_(payload),
      config_(fetcher_config),
      args_(args),
      channel_(channel),
      metrics_(ProtoFetcherMetrics::FromConfig(fetcher_config)) {
  // GET requests can't contain request body.
  CHECK(fetcher_config.method != FetcherConfig::Method::kGet ||
        payload.request_body.empty())
      << "GET requests cannot set request_body in payload.";
  if (!fetcher_config.access_token_config.has_value()) {
    // Starts the url loading without credentials.
    StartUrlLoader(url_loader_factory, std::nullopt);
    return;
  }

  fetcher_ = std::make_unique<ApiAccessTokenFetcher>(
      identity_manager, *fetcher_config.access_token_config);
  fetcher_->GetToken(
      base::BindOnce(&FetchProcess::OnAccessTokenFetchComplete,
                     base::Unretained(this),  // Unretained(.) is safe because
                                              // `this` owns `fetcher_`.
                     url_loader_factory));
}

FetchProcess::~FetchProcess() = default;

void FetchProcess::RecordMetrics(const ProtoFetcherStatus& status) const {
  if (!metrics_.has_value()) {
    return;
  }
  metrics_->RecordMetrics(status);
}

void FetchProcess::OnAccessTokenFetchComplete(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
        access_token) {
  if (!access_token.has_value()) {
    access_token_auth_error_ = access_token.error();
    ProtoFetcherStatus auth_error_status =
        ProtoFetcherStatus::GoogleServiceAuthError(access_token.error());
    CHECK(config_->access_token_config.has_value())
        << "Access token fetch underway, config is implied";
    if (config_->access_token_config->credentials_requirement ==
        AccessTokenConfig::CredentialsRequirement::kStrict) {
      // We've failed to fetch an access token and require one; fail with error.
      OnError(auth_error_status);
      return;
    }
    RecordMetrics(auth_error_status);
  }
  StartUrlLoader(url_loader_factory, base::OptionalFromExpected(access_token));
}

void FetchProcess::StartUrlLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::optional<signin::AccessTokenInfo> access_token_info) {
  simple_url_loader_ = InitializeSimpleUrlLoader(
      access_token_info, config_.get(), args_, channel_, payload_);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(
          &FetchProcess::OnSimpleUrlLoaderComplete,
          base::Unretained(this),  // Unretained(.) is safe because
                                   // `this` owns `simple_url_loader_`.
          url_loader_factory));
}

void FetchProcess::OnSimpleUrlLoaderComplete(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::optional<std::string> response_body) {
  // In best-effort mode we must retry on auth error in order that the request
  // can proceed without credentials.
  if (config_->access_token_config.has_value() &&
      config_->access_token_config->credentials_requirement ==
          AccessTokenConfig::CredentialsRequirement::kBestEffort &&
      HasHttpAuthErrorResponse(*simple_url_loader_) &&
      !triggered_retry_on_http_auth_error_) {
    // The server has rejected our credentials.
    // Mark the access token as invalid, and retry the request.
    CHECK(fetcher_) << "Retrying means that the was access token fetch";
    fetcher_->InvalidateToken();

    // Trigger a single retry (another retry is impossible).
    triggered_retry_on_http_auth_error_ = true;
    // Url loader initialized without access token on retry.
    simple_url_loader_ =
        InitializeSimpleUrlLoader(/*access_token_info=*/std::nullopt,
                                  config_.get(), args_, channel_, payload_);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindOnce(
            &FetchProcess::OnSimpleUrlLoaderComplete,
            base::Unretained(this),  // Unretained(.) is safe because
                                     // `this` owns `simple_url_loader_`.
            url_loader_factory));
    return;
  }

  if (!IsLoadingSuccessful(*simple_url_loader_) ||
      !HasHttpOkResponse(*simple_url_loader_)) {
    OnError(ProtoFetcherStatus::HttpStatusOrNetError(
        HttpStatusOrNetError(*simple_url_loader_)));
    return;
  }

  OnResponse(std::move(response_body));
}

bool ConfiguresFetcherWithoutEndUserCredentials(
    const FetcherConfig& fetcher_config) {
  return !fetcher_config.access_token_config.has_value() ||
         fetcher_config.access_token_config->credentials_requirement ==
             AccessTokenConfig::CredentialsRequirement::kBestEffort;
}

}  // namespace supervised_user
