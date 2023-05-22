// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_external_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_access_token_fetcher.h"
#include "components/supervised_user/core/browser/kids_external_fetcher_config.h"
#include "components/supervised_user/core/browser/kids_external_fetcher_requests.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {
// Controls the retry count of the simple url loader.
const int kNumFamilyInfoFetcherRetries = 1;

using ::base::BindOnce;
using ::base::JoinString;
using ::base::StrCat;
using ::base::StringPiece;
using ::base::StringPrintf;
using ::base::TimeDelta;
using ::base::TimeTicks;
using ::base::UmaHistogramEnumeration;
using ::base::UmaHistogramSparse;
using ::base::UmaHistogramTimes;
using ::base::Unretained;
using ::network::ResourceRequest;
using ::signin::IdentityManager;

bool IsLoadingSuccessful(const network::SimpleURLLoader& loader) {
  return loader.NetError() == net::OK;
}

bool HasHttpOkResponse(const network::SimpleURLLoader& loader) {
  if (!loader.ResponseInfo()) {
    return false;
  }
  if (!loader.ResponseInfo()->headers) {
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
  // Do not use StringPiece with StringPrintf, see crbug/1444165
  return base::StrCat(
      {supervised_user::kAuthorizationHeader, " ", access_token_info.token});
}

// TODO(b/276898959): Support payload for POST requests.
std::unique_ptr<network::SimpleURLLoader> InitializeSimpleUrlLoader(
    const signin::AccessTokenInfo access_token_info,
    const supervised_user::FetcherConfig& fetcher_config,
    const GURL& url) {
  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>();
  resource_request->url = url;
  resource_request->method = fetcher_config.GetHttpMethod();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      CreateAuthorizationHeader(access_token_info));
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       fetcher_config.traffic_annotation());
  simple_url_loader->SetRetryOptions(
      kNumFamilyInfoFetcherRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  return simple_url_loader;
}

// A fetcher with underlying network::SharedURLLoaderFactory.
// Internally, it's a two-phase process: first the access token is fetched, and
// if applicable, the remote service is called and the response is processed.
template <typename Request, typename Response>
class FetcherImpl final : public KidsExternalFetcher<Request, Response> {
 private:
  using Callback = typename KidsExternalFetcher<Request, Response>::Callback;

 public:
  FetcherImpl() = delete;
  explicit FetcherImpl(
      IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const supervised_user::FetcherConfig& fetcher_config,
      Callback callback)
      : config_(fetcher_config) {
    access_token_fetcher_ = std::make_unique<KidsAccessTokenFetcher>(
        identity_manager,
        BindOnce(&FetcherImpl::OnAccessTokenFetchComplete, Unretained(this),
                 url_loader_factory,
                 std::move(callback)));  // Unretained(.) is safe because `this`
                                         // owns `access_token_fetcher_`.
  }

  // Not copyable.
  FetcherImpl(const FetcherImpl&) = delete;
  FetcherImpl& operator=(const FetcherImpl&) = delete;

 private:
  void RecordStabilityMetrics(TimeDelta latency,
                              KidsExternalFetcherStatus status) {
    UmaHistogramEnumeration(GetMetricKey("Status"), status.state());
    UmaHistogramTimes(GetMetricKey("Latency"), latency);
    UmaHistogramTimes(GetMetricKey("Latency", status.ToMetricEnumLabel()),
                      latency);
  }

  void WrapCallbackWithMetrics(Callback callback,
                               TimeTicks start_time,
                               KidsExternalFetcherStatus status,
                               std::unique_ptr<Response> response) {
    TimeDelta latency = TimeTicks::Now() - start_time;
    RecordStabilityMetrics(latency, status);

    // Record additional metrics for various failures.
    if (status.state() ==
        KidsExternalFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR) {
      UmaHistogramSparse(GetMetricKey("HttpStatusOrNetError"),
                         status.http_status_or_net_error().value());
    }

    DCHECK(
        callback);  // https://chromium.googlesource.com/chromium/src/+/main/docs/callback.md#creating-a-callback-that-does-nothing
    std::move(callback).Run(status, std::move(response));
  }

  std::string GetMetricKey(StringPiece metric_id) const {
    return JoinString({config_.histogram_basename, metric_id}, ".");
  }
  std::string GetMetricKey(StringPiece metric_id,
                           StringPiece metric_suffix) const {
    return JoinString({config_.histogram_basename, metric_id, metric_suffix},
                      ".");
  }

  // First phase of fetching done: the access token response is ready.
  void OnAccessTokenFetchComplete(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Callback callback,
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
          access_token) {
    DCHECK(
        callback);  // https://chromium.googlesource.com/chromium/src/+/main/docs/callback.md#creating-a-callback-that-does-nothing

    Callback callback_with_metrics =
        BindOnce(&FetcherImpl::WrapCallbackWithMetrics, Unretained(this),
                 std::move(callback), TimeTicks::Now());

    if (!access_token.has_value()) {
      std::move(callback_with_metrics)
          .Run(KidsExternalFetcherStatus::GoogleServiceAuthError(
                   access_token.error()),
               std::make_unique<Response>());
      return;
    }

    // TODO(b/276898959): add optional payload for POST requests.
    simple_url_loader_ = InitializeSimpleUrlLoader(
        access_token.value(), config_,
        supervised_user::CreateRequestUrl<Request>(config_));

    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        BindOnce(
            &FetcherImpl::OnSimpleUrlLoaderComplete, Unretained(this),
            std::move(
                callback_with_metrics)));  // Unretained(.) is safe because
                                           // `this` owns `simple_url_loader_`.
  }

  // Second phase of fetching done: the remote service responded.
  void OnSimpleUrlLoaderComplete(Callback callback,
                                 std::unique_ptr<std::string> response_body) {
    if (!IsLoadingSuccessful(*simple_url_loader_) ||
        !HasHttpOkResponse(*simple_url_loader_)) {
      std::move(callback).Run(KidsExternalFetcherStatus::HttpStatusOrNetError(
                                  HttpStatusOrNetError(*simple_url_loader_)),
                              nullptr);
      return;
    }

    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      std::move(callback).Run(KidsExternalFetcherStatus::InvalidResponse(),
                              nullptr);
      return;
    }

    CHECK(response) << "KidsExternalFetcherStatus::Ok implies non-empty "
                       "response (which is always a valid message).";
    std::move(callback).Run(std::move(KidsExternalFetcherStatus::Ok()),
                            std::move(response));
  }

  std::unique_ptr<KidsAccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  const supervised_user::FetcherConfig config_;
};
}  // namespace

// Main constructor, referenced by the rest.
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    State state,
    class GoogleServiceAuthError google_service_auth_error)
    : state_(state), google_service_auth_error_(google_service_auth_error) {}
KidsExternalFetcherStatus::~KidsExternalFetcherStatus() = default;

KidsExternalFetcherStatus::KidsExternalFetcherStatus(State state)
    : state_(state) {
  DCHECK(state != State::GOOGLE_SERVICE_AUTH_ERROR);
}
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    HttpStatusOrNetErrorType http_status_or_net_error)
    : state_(State::HTTP_STATUS_OR_NET_ERROR),
      http_status_or_net_error_(http_status_or_net_error) {}
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    class GoogleServiceAuthError google_service_auth_error)
    : KidsExternalFetcherStatus(GOOGLE_SERVICE_AUTH_ERROR,
                                google_service_auth_error) {}

KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    const KidsExternalFetcherStatus& other) = default;
KidsExternalFetcherStatus& KidsExternalFetcherStatus::operator=(
    const KidsExternalFetcherStatus& other) = default;

KidsExternalFetcherStatus KidsExternalFetcherStatus::Ok() {
  return KidsExternalFetcherStatus(State::OK);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::GoogleServiceAuthError(
    class GoogleServiceAuthError error) {
  return KidsExternalFetcherStatus(error);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::HttpStatusOrNetError(
    int http_status_or_net_error) {
  return KidsExternalFetcherStatus(
      HttpStatusOrNetErrorType(http_status_or_net_error));
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::InvalidResponse() {
  return KidsExternalFetcherStatus(State::INVALID_RESPONSE);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::DataError() {
  return KidsExternalFetcherStatus(State::DATA_ERROR);
}

bool KidsExternalFetcherStatus::IsOk() const {
  return state_ == State::OK;
}
bool KidsExternalFetcherStatus::IsTransientError() const {
  if (state_ == State::HTTP_STATUS_OR_NET_ERROR) {
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsTransientError();
  }
  return false;
}
bool KidsExternalFetcherStatus::IsPersistentError() const {
  if (state_ == State::INVALID_RESPONSE) {
    return true;
  }
  if (state_ == State::DATA_ERROR) {
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsPersistentError();
  }
  return false;
}

std::string KidsExternalFetcherStatus::ToString() const {
  switch (state_) {
    case KidsExternalFetcherStatus::OK:
      return "KidsExternalFetcherStatus::OK";
    case KidsExternalFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR:
      return base::StrCat(
          {"KidsExternalFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR: ",
           google_service_auth_error().ToString()});
    case KidsExternalFetcherStatus::HTTP_STATUS_OR_NET_ERROR:
      return base::StringPrintf(
          "KidsExternalFetcherStatus::HTTP_STATUS_OR_NET_ERROR: %d",
          http_status_or_net_error_.value());
    case KidsExternalFetcherStatus::INVALID_RESPONSE:
      return "KidsExternalFetcherStatus::INVALID_RESPONSE";
    case KidsExternalFetcherStatus::DATA_ERROR:
      return "KidsExternalFetcherStatus::DATA_ERROR";
  }
}

// The returned value must match one of the labels in
// chromium/src/tools/metrics/histograms/enums.xml://enum[@name='KidsExternalFetcherStatus'],
// and should be reflected in tokens in histogram defined for this fetcher.
// See example at
// tools/metrics/histograms/metadata/signin/histograms.xml://histogram[@name='Signin.ListFamilyMembersRequest.{Status}.*']
std::string KidsExternalFetcherStatus::ToMetricEnumLabel() const {
  switch (state_) {
    case KidsExternalFetcherStatus::OK:
      return "NoError";
    case KidsExternalFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR:
      return "AuthError";
    case KidsExternalFetcherStatus::HTTP_STATUS_OR_NET_ERROR:
      return "HttpStatusOrNetError";
    case KidsExternalFetcherStatus::INVALID_RESPONSE:
      return "ParseError";
    case KidsExternalFetcherStatus::DATA_ERROR:
      return "DataError";
  }
}

KidsExternalFetcherStatus::State KidsExternalFetcherStatus::state() const {
  return state_;
}
KidsExternalFetcherStatus::HttpStatusOrNetErrorType
KidsExternalFetcherStatus::http_status_or_net_error() const {
  return http_status_or_net_error_;
}

const GoogleServiceAuthError&
KidsExternalFetcherStatus::google_service_auth_error() const {
  return google_service_auth_error_;
}

// Fetcher factories.
std::unique_ptr<
    KidsExternalFetcher<kids_chrome_management::ListFamilyMembersRequest,
                        kids_chrome_management::ListFamilyMembersResponse>>
FetchListFamilyMembers(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    KidsExternalFetcher<
        kids_chrome_management::ListFamilyMembersRequest,
        kids_chrome_management::ListFamilyMembersResponse>::Callback callback,
    const supervised_user::FetcherConfig& config) {
  return std::make_unique<
      FetcherImpl<kids_chrome_management::ListFamilyMembersRequest,
                  kids_chrome_management::ListFamilyMembersResponse>>(
      identity_manager, url_loader_factory, config, std::move(callback));
}
