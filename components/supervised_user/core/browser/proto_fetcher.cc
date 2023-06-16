// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher.h"

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
#include "components/supervised_user/core/browser/api_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace supervised_user {
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
  return base::StrCat({kAuthorizationHeader, " ", access_token_info.token});
}

// Determines the response type. See go/system-parameters to see list of
// possible One Platform system params.
constexpr base::StringPiece kSystemParameters("alt=proto");

// Creates a requests for kids management api which is independent from the
// current profile (doesn't take Profile* parameter). It also adds query
// parameter that configures the remote endpoint to respond with a protocol
// buffer message.
GURL CreateRequestUrl(const FetcherConfig& config) {
  return GURL(config.service_endpoint)
      .Resolve(base::StrCat({config.service_path, "?", kSystemParameters}));
}

std::unique_ptr<network::SimpleURLLoader> InitializeSimpleUrlLoader(
    const signin::AccessTokenInfo access_token_info,
    const FetcherConfig& fetcher_config,
    const absl::optional<std::string>& payload) {
  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>();
  resource_request->url = CreateRequestUrl(fetcher_config);
  resource_request->method = fetcher_config.GetHttpMethod();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      CreateAuthorizationHeader(access_token_info));
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       fetcher_config.traffic_annotation());

  if (payload.has_value()) {
    simple_url_loader->AttachStringForUpload(*payload,
                                             "application/x-protobuf");
  }

  simple_url_loader->SetRetryOptions(
      kNumFamilyInfoFetcherRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  return simple_url_loader;
}

// A fetcher with underlying network::SharedURLLoaderFactory.
// Internally, it's a two-phase process: first the access token is fetched, and
// if applicable, the remote service is called and the response is processed.
template <typename Response>
class FetcherImpl final : public ProtoFetcher<Response> {
 private:
  using Callback = typename ProtoFetcher<Response>::Callback;

 public:
  FetcherImpl() = delete;
  FetcherImpl(IdentityManager& identity_manager,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const google::protobuf::MessageLite& request,
              const FetcherConfig& fetcher_config,
              Callback callback)
      : payload_(request.SerializeAsString()), config_(fetcher_config) {
    access_token_fetcher_ = std::make_unique<ApiAccessTokenFetcher>(
        identity_manager, fetcher_config,
        BindOnce(&FetcherImpl::OnAccessTokenFetchComplete, Unretained(this),
                 url_loader_factory,
                 std::move(callback)));  // Unretained(.) is safe because `this`
                                         // owns `access_token_fetcher_`.
  }
  FetcherImpl(IdentityManager& identity_manager,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const std::string& payload,
              const FetcherConfig& fetcher_config,
              Callback callback)
      : payload_(payload), config_(fetcher_config) {
    access_token_fetcher_ = std::make_unique<ApiAccessTokenFetcher>(
        identity_manager, fetcher_config,
        BindOnce(&FetcherImpl::OnAccessTokenFetchComplete, Unretained(this),
                 url_loader_factory,
                 std::move(callback)));  // Unretained(.) is safe because `this`
                                         // owns `access_token_fetcher_`.
  }

  // Not copyable.
  FetcherImpl(const FetcherImpl&) = delete;
  FetcherImpl& operator=(const FetcherImpl&) = delete;

 private:
  void RecordStabilityMetrics(TimeDelta latency, ProtoFetcherStatus status) {
    UmaHistogramEnumeration(GetMetricKey("Status"), status.state());
    UmaHistogramTimes(GetMetricKey("Latency"), latency);
    UmaHistogramTimes(GetMetricKey("Latency", status.ToMetricEnumLabel()),
                      latency);
  }

  void WrapCallbackWithMetrics(Callback callback,
                               TimeTicks start_time,
                               ProtoFetcherStatus status,
                               std::unique_ptr<Response> response) {
    TimeDelta latency = TimeTicks::Now() - start_time;
    RecordStabilityMetrics(latency, status);

    // Record additional metrics for various failures.
    if (status.state() == ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR) {
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
          .Run(ProtoFetcherStatus::GoogleServiceAuthError(access_token.error()),
               std::make_unique<Response>());
      return;
    }

    simple_url_loader_ = InitializeSimpleUrlLoader(
        access_token.value(), config_, GetRequestPayload());

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
      std::move(callback).Run(ProtoFetcherStatus::HttpStatusOrNetError(
                                  HttpStatusOrNetError(*simple_url_loader_)),
                              nullptr);
      return;
    }

    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      std::move(callback).Run(ProtoFetcherStatus::InvalidResponse(), nullptr);
      return;
    }

    CHECK(response) << "ProtoFetcherStatus::Ok implies non-empty response "
                       "(which is always a valid message).";
    std::move(callback).Run(std::move(ProtoFetcherStatus::Ok()),
                            std::move(response));
  }

  // Returns payload when it's eligible for the request type.
  absl::optional<std::string> GetRequestPayload() const {
    if (config_.method == FetcherConfig::Method::kGet) {
      CHECK(payload_.empty()) << "Unexpected payload in GET request";
      return absl::nullopt;
    }
    return payload_;
  }

  std::unique_ptr<ApiAccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  const std::string payload_;
  const FetcherConfig config_;
};

// Wraps FetcherImpl deferring its startup until explicitly invoked.
// It is useful to have fetcher with self-reference, which will destroy
// themselves upon completion.
template <typename Response>
class DeferredFetcherImpl final : public DeferredProtoFetcher<Response> {
 private:
  using Callback = typename DeferredProtoFetcher<Response>::Callback;

 public:
  DeferredFetcherImpl() = delete;
  DeferredFetcherImpl(
      IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const google::protobuf::MessageLite& request,
      const FetcherConfig& fetcher_config)
      : payload_(request.SerializeAsString()),
        identity_manager_(identity_manager),
        url_loader_factory_(url_loader_factory),
        config_(fetcher_config) {}

  void Start(Callback callback) override {
    fetcher_ = std::make_unique<FetcherImpl<Response>>(
        identity_manager_, url_loader_factory_, payload_, config_,
        std::move(callback));
  }

 private:
  std::unique_ptr<FetcherImpl<Response>> fetcher_;
  std::string payload_;
  IdentityManager& identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const FetcherConfig config_;
};

using ClassifyUrlFetcher =
    ProtoFetcher<kids_chrome_management::ClassifyUrlResponse>;
using ListFamilyMembersFetcher =
    ProtoFetcher<kids_chrome_management::ListFamilyMembersResponse>;
using PermissionRequestFetcher = DeferredProtoFetcher<
    kids_chrome_management::CreatePermissionRequestResponse>;
}  // namespace

// Main constructor, referenced by the rest.
ProtoFetcherStatus::ProtoFetcherStatus(
    State state,
    class GoogleServiceAuthError google_service_auth_error)
    : state_(state), google_service_auth_error_(google_service_auth_error) {}
ProtoFetcherStatus::~ProtoFetcherStatus() = default;

ProtoFetcherStatus::ProtoFetcherStatus(State state) : state_(state) {
  DCHECK(state != State::GOOGLE_SERVICE_AUTH_ERROR);
}
ProtoFetcherStatus::ProtoFetcherStatus(
    HttpStatusOrNetErrorType http_status_or_net_error)
    : state_(State::HTTP_STATUS_OR_NET_ERROR),
      http_status_or_net_error_(http_status_or_net_error) {}
ProtoFetcherStatus::ProtoFetcherStatus(
    class GoogleServiceAuthError google_service_auth_error)
    : ProtoFetcherStatus(GOOGLE_SERVICE_AUTH_ERROR, google_service_auth_error) {
}

ProtoFetcherStatus::ProtoFetcherStatus(const ProtoFetcherStatus& other) =
    default;
ProtoFetcherStatus& ProtoFetcherStatus::operator=(
    const ProtoFetcherStatus& other) = default;

ProtoFetcherStatus ProtoFetcherStatus::Ok() {
  return ProtoFetcherStatus(State::OK);
}
ProtoFetcherStatus ProtoFetcherStatus::GoogleServiceAuthError(
    class GoogleServiceAuthError error) {
  return ProtoFetcherStatus(error);
}
ProtoFetcherStatus ProtoFetcherStatus::HttpStatusOrNetError(
    int http_status_or_net_error) {
  return ProtoFetcherStatus(HttpStatusOrNetErrorType(http_status_or_net_error));
}
ProtoFetcherStatus ProtoFetcherStatus::InvalidResponse() {
  return ProtoFetcherStatus(State::INVALID_RESPONSE);
}
ProtoFetcherStatus ProtoFetcherStatus::DataError() {
  return ProtoFetcherStatus(State::DATA_ERROR);
}

bool ProtoFetcherStatus::IsOk() const {
  return state_ == State::OK;
}
bool ProtoFetcherStatus::IsTransientError() const {
  if (state_ == State::HTTP_STATUS_OR_NET_ERROR) {
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsTransientError();
  }
  return false;
}
bool ProtoFetcherStatus::IsPersistentError() const {
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

std::string ProtoFetcherStatus::ToString() const {
  switch (state_) {
    case ProtoFetcherStatus::OK:
      return "ProtoFetcherStatus::OK";
    case ProtoFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR:
      return base::StrCat({"ProtoFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR: ",
                           google_service_auth_error().ToString()});
    case ProtoFetcherStatus::HTTP_STATUS_OR_NET_ERROR:
      return base::StringPrintf(
          "ProtoFetcherStatus::HTTP_STATUS_OR_NET_ERROR: %d",
          http_status_or_net_error_.value());
    case ProtoFetcherStatus::INVALID_RESPONSE:
      return "ProtoFetcherStatus::INVALID_RESPONSE";
    case ProtoFetcherStatus::DATA_ERROR:
      return "ProtoFetcherStatus::DATA_ERROR";
  }
}

// The returned value must match one of the labels in
// chromium/src/tools/metrics/histograms/enums.xml://enum[@name='ProtoFetcherStatus'],
// and should be reflected in tokens in histogram defined for this fetcher.
// See example at
// tools/metrics/histograms/metadata/signin/histograms.xml://histogram[@name='Signin.ListFamilyMembersRequest.{Status}.*']
std::string ProtoFetcherStatus::ToMetricEnumLabel() const {
  switch (state_) {
    case ProtoFetcherStatus::OK:
      return "NoError";
    case ProtoFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR:
      return "AuthError";
    case ProtoFetcherStatus::HTTP_STATUS_OR_NET_ERROR:
      return "HttpStatusOrNetError";
    case ProtoFetcherStatus::INVALID_RESPONSE:
      return "ParseError";
    case ProtoFetcherStatus::DATA_ERROR:
      return "DataError";
  }
}

ProtoFetcherStatus::State ProtoFetcherStatus::state() const {
  return state_;
}
ProtoFetcherStatus::HttpStatusOrNetErrorType
ProtoFetcherStatus::http_status_or_net_error() const {
  return http_status_or_net_error_;
}

const GoogleServiceAuthError& ProtoFetcherStatus::google_service_auth_error()
    const {
  return google_service_auth_error_;
}

// Fetcher factories.
std::unique_ptr<ClassifyUrlFetcher> ClassifyURL(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kids_chrome_management::ClassifyUrlRequest& request,
    ClassifyUrlFetcher::Callback callback,
    const FetcherConfig& config) {
  return std::make_unique<
      FetcherImpl<kids_chrome_management::ClassifyUrlResponse>>(
      identity_manager, url_loader_factory, request, config,
      std::move(callback));
}

std::unique_ptr<ListFamilyMembersFetcher> FetchListFamilyMembers(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ListFamilyMembersFetcher::Callback callback,
    const FetcherConfig& config) {
  return std::make_unique<
      FetcherImpl<kids_chrome_management::ListFamilyMembersResponse>>(
      identity_manager, url_loader_factory,
      kids_chrome_management::ListFamilyMembersRequest(), config,
      std::move(callback));
}

std::unique_ptr<PermissionRequestFetcher> CreatePermissionRequestFetcher(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kids_chrome_management::PermissionRequest& request,
    const FetcherConfig& config) {
  return std::make_unique<DeferredFetcherImpl<
      kids_chrome_management::CreatePermissionRequestResponse>>(
      identity_manager, url_loader_factory, request, config);
}

}  // namespace supervised_user
