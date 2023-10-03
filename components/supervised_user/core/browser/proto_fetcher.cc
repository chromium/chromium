// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/api_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto/test.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {
// Controls the retry count of the simple url loader.
const int kUrlLoaderRetryCount = 1;

using ::base::BindOnce;
using ::base::ElapsedTimer;
using ::base::JoinString;
using ::base::StrCat;
using ::base::StringPiece;
using ::base::StringPrintf;
using ::base::TimeDelta;
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
constexpr StringPiece kSystemParameters("alt=proto");

// Creates a requests for kids management api which is independent from the
// current profile (doesn't take Profile* parameter). It also adds query
// parameter that configures the remote endpoint to respond with a protocol
// buffer message.
GURL CreateRequestUrl(const FetcherConfig& config) {
  return GURL(config.service_endpoint.Get())
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
      kUrlLoaderRetryCount, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  return simple_url_loader;
}

// A stopwatch with two functions:
// * measure total elapsed time,
// * measure lap time (with automatic resetting after each lap).
// The stopwatch is created started.
class Stopwatch {
 public:
  // Time since start of last lap. Resets the lap timer.
  TimeDelta Lap() {
    TimeDelta lap = lap_timer_.Elapsed();
    lap_timer_ = ElapsedTimer();
    return lap;
  }

  // Time since start of last lap.
  TimeDelta Elapsed() const { return elapsed_timer_.Elapsed(); }

 private:
  ElapsedTimer elapsed_timer_;
  ElapsedTimer lap_timer_;
};

// Encapsulates metric functionalities.
class Metrics {
 public:
  enum class MetricType {
    kStatus,
    kLatency,
    kHttpStatusOrNetError,
    kRetryCount,
    kAccessTokenLatency,
    kApiLatency,
  };

  Metrics() = delete;
  explicit Metrics(StringPiece basename) : basename_(basename) {}

  void RecordStatus(ProtoFetcherStatus status) const {
    base::UmaHistogramEnumeration(GetFullHistogramName(MetricType::kStatus),
                                  status.state());
  }

  void RecordLatency() const {
    base::UmaHistogramTimes(GetFullHistogramName(MetricType::kLatency),
                            stopwatch_.Elapsed());
  }

  void RecordAccessTokenLatency(
      GoogleServiceAuthError::State auth_error_state) {
    base::UmaHistogramTimes(
        GetFullHistogramName(MetricType::kAccessTokenLatency, auth_error_state),
        stopwatch_.Lap());
  }

  void RecordApiLatency(
      ProtoFetcherStatus::HttpStatusOrNetErrorType http_status_or_net_error) {
    base::UmaHistogramTimes(
        GetFullHistogramName(MetricType::kApiLatency, http_status_or_net_error),
        stopwatch_.Lap());
  }

  virtual void RecordStatusLatency(ProtoFetcherStatus status) const {
    base::UmaHistogramTimes(GetFullHistogramName(MetricType::kLatency, status),
                            stopwatch_.Elapsed());
  }

  void RecordHttpStatusOrNetError(ProtoFetcherStatus status) const {
    CHECK(status.state() ==
          ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
    base::UmaHistogramSparse(
        GetFullHistogramName(MetricType::kHttpStatusOrNetError),
        status.http_status_or_net_error().value());
  }

 protected:
  // Translates top-level metric type into a string. ::ToMetricEnumLabel
  // translates statuses for per-status latency tracking.
  virtual StringPiece GetMetricKey(MetricType metric_type) const {
    switch (metric_type) {
      case MetricType::kStatus:
        return "Status";
      case MetricType::kLatency:
        return "Latency";
      case MetricType::kHttpStatusOrNetError:
        return "HttpStatusOrNetError";
      case MetricType::kAccessTokenLatency:
        return "AccessTokenLatency";
      case MetricType::kApiLatency:
        return "ApiLatency";
      case MetricType::kRetryCount:
        NOTREACHED_NORETURN();
      default:
        NOTREACHED_NORETURN();
    }
  }

  // Returns fully-qualified name of histogram for specified metric_type.
  std::string GetFullHistogramName(MetricType metric_type) const {
    return JoinString({basename_, GetMetricKey(metric_type)}, ".");
  }

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-status values.
  std::string GetFullHistogramName(MetricType metric_type,
                                   ProtoFetcherStatus status) const {
    return JoinString(
        {basename_, ToMetricEnumLabel(status), GetMetricKey(metric_type)}, ".");
  }

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-authentication status values.
  std::string GetFullHistogramName(
      MetricType metric_type,
      GoogleServiceAuthError::State auth_error_state) const {
    CHECK_EQ(auth_error_state, GoogleServiceAuthError::State::NONE)
        << "Only authenticated case is supported.";
    return JoinString({basename_, "NONE", GetMetricKey(metric_type)}, ".");
  }

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-net-or-http error values.
  std::string GetFullHistogramName(MetricType metric_type,
                                   ProtoFetcherStatus::HttpStatusOrNetErrorType
                                       http_status_or_net_error) const {
    CHECK_EQ(http_status_or_net_error,
             ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_OK))
        << "Only successful api call case is supported.";
    return JoinString({basename_, "HTTP_OK", GetMetricKey(metric_type)}, ".");
  }

 private:
  // The returned value must match one of the labels in
  // chromium/src/tools/metrics/histograms/enums.xml://enum[@name='ProtoFetcherStatus'],
  // and should be reflected in tokens in histogram defined for this fetcher.
  // See example at
  // tools/metrics/histograms/metadata/signin/histograms.xml://histogram[@name='Signin.ListFamilyMembersRequest.{Status}.*']
  static std::string ToMetricEnumLabel(ProtoFetcherStatus status) {
    switch (status.state()) {
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
      default:
        NOTREACHED_NORETURN();
    }
  }

  StringPiece basename_;
  Stopwatch stopwatch_;
};

// Metrics for retrying fetchers, which are aggregating individual
// fetchers.
class OverallMetrics final : public Metrics {
 public:
  OverallMetrics() = delete;
  explicit OverallMetrics(StringPiece basename) : Metrics(basename) {}

  // Per-status latency is not defined for OverallMetrics.
  void RecordStatusLatency(ProtoFetcherStatus status) const override {
    NOTIMPLEMENTED();
  }

 protected:
  StringPiece GetMetricKey(MetricType metric_type) const override {
    switch (metric_type) {
      case MetricType::kStatus:
        return "OverallStatus";
      case MetricType::kLatency:
        return "OverallLatency";
      case MetricType::kHttpStatusOrNetError:
        NOTREACHED_NORETURN();
      case MetricType::kRetryCount:
        return "RetryCount";
      default:
        NOTREACHED_NORETURN();
    }
  }

 public:
  void RecordRetryCount(int count) const {
    // It's a prediction that it will take less than 100 retries to get a
    // decisive response. Double exponential backoff set at 4 hour limit
    // shouldn't exhaust this limit too soon.
    base::UmaHistogramCounts100(GetFullHistogramName(MetricType::kRetryCount),
                                count);
  }
};

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
      : FetcherImpl(identity_manager,
                    url_loader_factory,
                    request.SerializeAsString(),
                    fetcher_config,
                    std::move(callback)) {}

  FetcherImpl(IdentityManager& identity_manager,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              StringPiece payload,
              const FetcherConfig& fetcher_config,
              Callback callback)
      : payload_(payload),
        config_(fetcher_config),
        metrics_(fetcher_config.histogram_basename),
        fetcher_(identity_manager,
                 fetcher_config.access_token_config,
                 BindOnce(&FetcherImpl::OnAccessTokenFetchComplete,
                          Unretained(this),  // Unretained(.) is safe because
                                             // `this` owns `fetcher_`.
                          url_loader_factory,
                          std::move(callback))) {}

  // Not copyable.
  FetcherImpl(const FetcherImpl&) = delete;
  FetcherImpl& operator=(const FetcherImpl&) = delete;

 protected:
  std::string GetMetricKey(StringPiece metric_id) const {
    return JoinString({config_.histogram_basename, metric_id}, ".");
  }
  std::string GetMetricKey(StringPiece metric_id,
                           StringPiece metric_suffix) const {
    return JoinString({config_.histogram_basename, metric_id, metric_suffix},
                      ".");
  }

 private:
  void RecordMetrics(ProtoFetcherStatus status) {
    metrics_.RecordStatus(status);
    metrics_.RecordLatency();
    metrics_.RecordStatusLatency(status);

    // Record additional metrics for various failures.
    if (status.state() == ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR) {
      metrics_.RecordHttpStatusOrNetError(status);
    }
  }

  // First phase of fetching done: the access token response is ready.
  void OnAccessTokenFetchComplete(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Callback callback,
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
          access_token) {
    if (!access_token.has_value()) {
      OnError(std::move(callback),
              ProtoFetcherStatus::GoogleServiceAuthError(access_token.error()));
      return;
    }

    metrics_.RecordAccessTokenLatency(GoogleServiceAuthError::State::NONE);

    simple_url_loader_ = InitializeSimpleUrlLoader(
        access_token.value(), config_, GetRequestPayload());
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        BindOnce(&FetcherImpl::OnSimpleUrlLoaderComplete, Unretained(this),
                 std::move(callback)));  // Unretained(.) is safe because
                                         // `this` owns `simple_url_loader_`.
  }

  // Second phase of fetching done: the remote service responded.
  void OnSimpleUrlLoaderComplete(Callback callback,
                                 std::unique_ptr<std::string> response_body) {
    if (!IsLoadingSuccessful(*simple_url_loader_) ||
        !HasHttpOkResponse(*simple_url_loader_)) {
      OnError(std::move(callback),
              ProtoFetcherStatus::HttpStatusOrNetError(
                  HttpStatusOrNetError(*simple_url_loader_)));
      return;
    }

    metrics_.RecordApiLatency(
        ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_OK));

    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      OnError(std::move(callback), ProtoFetcherStatus::InvalidResponse());
      return;
    }

    OnSuccess(std::move(callback), std::move(response));
  }

  // Returns payload when it's eligible for the request type.
  absl::optional<std::string> GetRequestPayload() const {
    if (config_.method == FetcherConfig::Method::kGet) {
      return absl::nullopt;
    }
    return payload_;
  }

  void OnError(Callback callback, ProtoFetcherStatus status) {
    RecordMetrics(status);
    std::move(callback).Run(status, nullptr);
  }

  void OnSuccess(Callback callback, std::unique_ptr<Response> response) {
    CHECK(response) << "ProtoFetcherStatus::Ok implies non-empty response "
                       "(which is always a valid message).";
    RecordMetrics(ProtoFetcherStatus::Ok());
    std::move(callback).Run(ProtoFetcherStatus::Ok(), std::move(response));
  }

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  const std::string payload_;
  const FetcherConfig config_;
  Metrics metrics_;

  // Entrypoint of the fetch process, which starts with ApiAccessToken access
  // followed by a request made with SimpleURLLoader. Purposely made last field
  // should it depend on other members of this class.
  ApiAccessTokenFetcher fetcher_;
};

// Wraps FetcherImpl deferring its startup until explicitly invoked. This is the
// preferred type of fetcher instances, giving the most of flexibility.
template <typename Response>
class DeferredFetcherImpl : public DeferredProtoFetcher<Response> {
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
        identity_manager_.get(), url_loader_factory_, payload_, config_,
        std::move(callback));
  }
  void Stop() override {
    CHECK(fetcher_) << "Only started fetcher can be stopped.";
    fetcher_.reset();
  }

 private:
  std::unique_ptr<FetcherImpl<Response>> fetcher_;
  std::string payload_;
  const raw_ref<IdentityManager, LeakedDanglingUntriaged> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const FetcherConfig config_;
};

// A subtype of DeferredFetcher that will take retrying strategy as specified in
// FetcherConfig::backoff_policy.
//
// The retries are only performed on transient errors (see ::ShouldRetry).
template <typename Response>
class RetryingFetcherImpl final : public DeferredFetcherImpl<Response> {
 public:
  RetryingFetcherImpl() = delete;

  RetryingFetcherImpl(
      IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const google::protobuf::MessageLite& request,
      const FetcherConfig& fetcher_config,
      const net::BackoffEntry::Policy& backoff_policy)
      : DeferredFetcherImpl<Response>(identity_manager,
                                      url_loader_factory,
                                      request,
                                      fetcher_config),
        backoff_entry_(&backoff_policy),
        metrics_(fetcher_config.histogram_basename) {}

  // Not copyable.
  RetryingFetcherImpl(const RetryingFetcherImpl&) = delete;
  RetryingFetcherImpl& operator=(const RetryingFetcherImpl&) = delete;

  void Start(ProtoFetcher<Response>::Callback callback) override {
    callback_ = std::move(callback);
    Retry();
  }
  void Stop() override {
    DeferredFetcherImpl<Response>::Stop();
    timer_.Stop();
  }

 private:
  void Retry() {
    retry_count_++;
    DeferredFetcherImpl<Response>::Start(base::BindOnce(
        &RetryingFetcherImpl<Response>::OnResponse, Unretained(this)));
  }

  bool ShouldRetry(ProtoFetcherStatus status) {
    return status.IsTransientError();
  }

  void OnResponse(ProtoFetcherStatus status,
                  std::unique_ptr<Response> response) {
    if (ShouldRetry(status)) {
      backoff_entry_.InformOfRequest(/*succeeded=*/false);
      timer_.Start(FROM_HERE, backoff_entry_.GetTimeUntilRelease(), this,
                   &RetryingFetcherImpl<Response>::Retry);
      return;
    }

    CHECK(callback_) << "Callback can be used only once.";
    backoff_entry_.InformOfRequest(/*succeeded=*/true);
    metrics_.RecordLatency();
    metrics_.RecordStatus(status);
    metrics_.RecordRetryCount(retry_count_);
    std::move(callback_).Run(status, std::move(response));
  }

  // Client callback.
  ProtoFetcher<Response>::Callback callback_;

  // Retry controls.
  base::OneShotTimer timer_;
  net::BackoffEntry backoff_entry_;
  int retry_count_{0};

  const OverallMetrics metrics_;
};

using ClassifyUrlFetcher =
    DeferredProtoFetcher<kids_chrome_management::ClassifyUrlResponse>;
using ListFamilyMembersFetcher =
    DeferredProtoFetcher<kids_chrome_management::ListFamilyMembersResponse>;
using PermissionRequestFetcher = DeferredProtoFetcher<
    kids_chrome_management::CreatePermissionRequestResponse>;
}  // namespace

// Constructs a fetcher that needs to be launched with ::Start(). The fetcher
// will be either one shot or retryable, depending on the
// FetcherConfig::backoff_policy setting.
template <typename Response>
std::unique_ptr<DeferredProtoFetcher<Response>> CreateFetcher(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const google::protobuf::MessageLite& request,
    const FetcherConfig& fetcher_config) {
  if (fetcher_config.backoff_policy.has_value()) {
    return std::make_unique<RetryingFetcherImpl<Response>>(
        identity_manager, url_loader_factory, request, fetcher_config,
        *fetcher_config.backoff_policy);
  } else {
    return std::make_unique<DeferredFetcherImpl<Response>>(
        identity_manager, url_loader_factory, request, fetcher_config);
  }
}

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

template <typename Request, typename Response>
ParallelFetchManager<Request, Response>::ParallelFetchManager(
    FetcherFactory fetcher_factory)
    : fetcher_factory_(fetcher_factory) {}

template <typename Request, typename Response>
void ParallelFetchManager<Request, Response>::Fetch(
    const Request& request,
    Fetcher::Callback callback) {
  CHECK(callback) << "Use base::DoNothing() instead of empty callback.";
  KeyType key = requests_in_flight_.Add(MakeFetcher(request));
  requests_in_flight_.Lookup(key)->Start(
      std::move(callback).Then(base::BindOnce(
          &ParallelFetchManager::Remove, weak_factory_.GetWeakPtr(), key)));
}

template <typename Request, typename Response>
void ParallelFetchManager<Request, Response>::Remove(KeyType key) {
  requests_in_flight_.Remove(key);
}

template <typename Request, typename Response>
std::unique_ptr<DeferredProtoFetcher<Response>>
ParallelFetchManager<Request, Response>::MakeFetcher(
    const Request& request) const {
  return fetcher_factory_.Run(request);
}

// Required, because users of ParallelFetchManager are externally linked.
template class ParallelFetchManager<
    kids_chrome_management::ClassifyUrlRequest,
    kids_chrome_management::ClassifyUrlResponse>;
template class ParallelFetchManager<
    kids_chrome_management::PermissionRequest,
    kids_chrome_management::CreatePermissionRequestResponse>;

std::unique_ptr<ClassifyUrlFetcher> CreateClassifyURLFetcher(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kids_chrome_management::ClassifyUrlRequest& request,
    const FetcherConfig& config) {
  return CreateFetcher<kids_chrome_management::ClassifyUrlResponse>(
      identity_manager, url_loader_factory, request, config);
}

std::unique_ptr<ListFamilyMembersFetcher> FetchListFamilyMembers(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ListFamilyMembersFetcher::Callback callback,
    const FetcherConfig& config) {
  std::unique_ptr<ListFamilyMembersFetcher> fetcher =
      CreateFetcher<kids_chrome_management::ListFamilyMembersResponse>(
          identity_manager, url_loader_factory,
          kids_chrome_management::ListFamilyMembersRequest(), config);
  fetcher->Start(std::move(callback));
  return fetcher;
}

std::unique_ptr<PermissionRequestFetcher> CreatePermissionRequestFetcher(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kids_chrome_management::PermissionRequest& request,
    const FetcherConfig& config) {
  return CreateFetcher<kids_chrome_management::CreatePermissionRequestResponse>(
      identity_manager, url_loader_factory, request, config);
}

std::unique_ptr<DeferredProtoFetcher<Response>> CreateTestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const Request& request,
    const FetcherConfig& config) {
  return CreateFetcher<Response>(identity_manager, url_loader_factory, request,
                                 config);
}

}  // namespace supervised_user
