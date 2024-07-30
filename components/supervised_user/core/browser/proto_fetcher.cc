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
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto/test.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/request_priority.h"
#include "net/http/http_status_code.h"
#include "proto_fetcher.h"
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
                      const FetcherConfig::PathArgs& args) {
  return GURL(config.service_endpoint.Get())
      .Resolve(
          base::StrCat({config.ServicePath(args), "?", kSystemParameters}));
}

std::unique_ptr<network::SimpleURLLoader> InitializeSimpleUrlLoader(
    const std::optional<signin::AccessTokenInfo> access_token_info,
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args,
    std::optional<version_info::Channel> channel,
    const std::optional<std::string>& payload) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = CreateRequestUrl(fetcher_config, args);
  resource_request->method = fetcher_config.GetHttpMethod();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->priority = fetcher_config.request_priority;

  if (access_token_info) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        CreateAuthorizationHeader(access_token_info.value()));
  } else {
    CHECK(channel);
    resource_request->headers.SetHeader(
        "X-Goog-Api-Key", *channel == version_info::Channel::STABLE
                              ? google_apis::GetAPIKey()
                              : google_apis::GetNonStableAPIKey());
  }

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

}  // namespace

// Main constructor, referenced by the rest.
ProtoFetcherStatus::ProtoFetcherStatus(
    State state,
    class GoogleServiceAuthError google_service_auth_error)
    : state_(state), google_service_auth_error_(google_service_auth_error) {}
ProtoFetcherStatus::~ProtoFetcherStatus() = default;

ProtoFetcherStatus::ProtoFetcherStatus(State state) : state_(state) {
  DCHECK_NE(state, State::GOOGLE_SERVICE_AUTH_ERROR);
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

base::TimeDelta Stopwatch::Lap() {
  base::TimeDelta lap = lap_timer_.Elapsed();
  lap_timer_ = base::ElapsedTimer();
  return lap;
}
base::TimeDelta Stopwatch::Elapsed() const {
  return elapsed_timer_.Elapsed();
}

Metrics::Metrics(std::string_view basename) : basename_(basename) {}
/* static */ std::optional<Metrics> Metrics::FromConfig(
    const FetcherConfig& config) {
  if (config.histogram_basename.has_value()) {
    return Metrics(*config.histogram_basename);
  }
  return std::nullopt;
}

void Metrics::RecordStatus(const ProtoFetcherStatus& status) const {
  base::UmaHistogramEnumeration(GetFullHistogramName(MetricType::kStatus),
                                status.state());
}

void Metrics::RecordLatency() const {
  base::UmaHistogramTimes(GetFullHistogramName(MetricType::kLatency),
                          stopwatch_.Elapsed());
}

void Metrics::RecordAccessTokenLatency(
    GoogleServiceAuthError::State auth_error_state) {
  base::UmaHistogramTimes(
      GetFullHistogramName(MetricType::kAccessTokenLatency, auth_error_state),
      stopwatch_.Lap());
}

void Metrics::RecordApiLatency(
    ProtoFetcherStatus::HttpStatusOrNetErrorType http_status_or_net_error) {
  base::UmaHistogramTimes(
      GetFullHistogramName(MetricType::kApiLatency, http_status_or_net_error),
      stopwatch_.Lap());
}

void Metrics::RecordStatusLatency(const ProtoFetcherStatus& status) const {
  base::UmaHistogramTimes(GetFullHistogramName(MetricType::kLatency, status),
                          stopwatch_.Elapsed());
}

void Metrics::RecordAuthError(const GoogleServiceAuthError& auth_error) const {
  base::UmaHistogramEnumeration(GetFullHistogramName(MetricType::kAuthError),
                                auth_error.state(),
                                GoogleServiceAuthError::NUM_STATES);
}

void Metrics::RecordHttpStatusOrNetError(
    const ProtoFetcherStatus& status) const {
  CHECK_EQ(status.state(), ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  base::UmaHistogramSparse(
      GetFullHistogramName(MetricType::kHttpStatusOrNetError),
      status.http_status_or_net_error().value());
}

std::string Metrics::GetMetricKey(MetricType metric_type) const {
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
    case MetricType::kAuthError:
      return "AuthError";
    case MetricType::kRetryCount:
      NOTREACHED_NORETURN();
    default:
      NOTREACHED_NORETURN();
  }
}

std::string Metrics::GetFullHistogramName(MetricType metric_type) const {
  return base::JoinString({basename_, GetMetricKey(metric_type)}, ".");
}

std::string Metrics::GetFullHistogramName(MetricType metric_type,
                                          ProtoFetcherStatus status) const {
  return base::JoinString(
      {basename_, ToMetricEnumLabel(status), GetMetricKey(metric_type)}, ".");
}

std::string Metrics::GetFullHistogramName(
    MetricType metric_type,
    GoogleServiceAuthError::State auth_error_state) const {
  CHECK_EQ(auth_error_state, GoogleServiceAuthError::State::NONE)
      << "Only authenticated case is supported.";
  return base::JoinString({basename_, "NONE", GetMetricKey(metric_type)}, ".");
}

std::string Metrics::GetFullHistogramName(
    MetricType metric_type,
    ProtoFetcherStatus::HttpStatusOrNetErrorType http_status_or_net_error)
    const {
  CHECK_EQ(http_status_or_net_error,
           ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_OK))
      << "Only successful api call case is supported.";
  return base::JoinString({basename_, "HTTP_OK", GetMetricKey(metric_type)},
                          ".");
}

std::string Metrics::ToMetricEnumLabel(const ProtoFetcherStatus& status) {
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

OverallMetrics::OverallMetrics(std::string_view basename) : Metrics(basename) {}
/* static */ std::optional<OverallMetrics> OverallMetrics::FromConfig(
    const FetcherConfig& config) {
  if (config.histogram_basename.has_value()) {
    return OverallMetrics(*config.histogram_basename);
  }
  return std::nullopt;
}

// Per-status latency is not defined for OverallMetrics.
void OverallMetrics::RecordStatusLatency(
    const ProtoFetcherStatus& status) const {
  NOTIMPLEMENTED();
}

std::string OverallMetrics::GetMetricKey(MetricType metric_type) const {
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

void OverallMetrics::RecordRetryCount(int count) const {
  // It's a prediction that it will take less than 100 retries to get a
  // decisive response. Double exponential backoff set at 4 hour limit
  // shouldn't exhaust this limit too soon.
  base::UmaHistogramCounts100(GetFullHistogramName(MetricType::kRetryCount),
                              count);
}

AbstractProtoFetcher::AbstractProtoFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view payload,
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args,
    std::optional<version_info::Channel> channel)
    : payload_(payload),
      config_(fetcher_config),
      args_(args),
      channel_(channel),
      metrics_(Metrics::FromConfig(fetcher_config)),
      fetcher_(identity_manager,
               fetcher_config.access_token_config,
               base::BindOnce(
                   &AbstractProtoFetcher::OnAccessTokenFetchComplete,
                   base::Unretained(this),  // Unretained(.) is safe because
                                            // `this` owns `fetcher_`.
                   url_loader_factory)) {}
AbstractProtoFetcher::~AbstractProtoFetcher() = default;
bool AbstractProtoFetcher::IsMetricsRecordingEnabled() const {
  return metrics_.has_value();
}

void AbstractProtoFetcher::RecordMetrics(const ProtoFetcherStatus& status) {
  if (!IsMetricsRecordingEnabled()) {
    return;
  }
  metrics_->RecordStatus(status);
  metrics_->RecordLatency();
  metrics_->RecordStatusLatency(status);

  if (access_token_auth_error_) {
    metrics_->RecordAuthError(access_token_auth_error_.value());
  }

  if (status.state() == ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR) {
    metrics_->RecordHttpStatusOrNetError(status);
  }
}

void AbstractProtoFetcher::OnAccessTokenFetchComplete(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
        access_token) {
  if (!access_token.has_value()) {
    access_token_auth_error_ = access_token.error();
    if (config_.access_token_config.credentials_requirement ==
        AccessTokenConfig::CredentialsRequirement::kStrict) {
      OnError(ProtoFetcherStatus::GoogleServiceAuthError(access_token.error()));
      return;
    }
  }

  if (IsMetricsRecordingEnabled()) {
    metrics_->RecordAccessTokenLatency(GoogleServiceAuthError::State::NONE);
  }

  simple_url_loader_ =
      InitializeSimpleUrlLoader(base::OptionalFromExpected(access_token),
                                config_, args_, channel_, GetRequestPayload());
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(
          &AbstractProtoFetcher::OnSimpleUrlLoaderComplete,
          base::Unretained(this)));  // Unretained(.) is safe because
                                     // `this` owns `simple_url_loader_`.
}

void AbstractProtoFetcher::OnSimpleUrlLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!IsLoadingSuccessful(*simple_url_loader_) ||
      !HasHttpOkResponse(*simple_url_loader_)) {
    OnError(ProtoFetcherStatus::HttpStatusOrNetError(
        HttpStatusOrNetError(*simple_url_loader_)));
    return;
  }

  if (IsMetricsRecordingEnabled()) {
    metrics_->RecordApiLatency(
        ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_OK));
  }
  OnResponse(std::move(response_body));
}

std::optional<std::string> AbstractProtoFetcher::GetRequestPayload() const {
  if (config_.method == FetcherConfig::Method::kGet) {
    return std::nullopt;
  }
  return payload_;
}

StatusFetcher::StatusFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view payload,
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args,
    std::optional<version_info::Channel> channel,
    Callback callback)
    : AbstractProtoFetcher(identity_manager,
                           url_loader_factory,
                           payload,
                           fetcher_config,
                           args,
                           channel),
      callback_(std::move(callback)) {}
StatusFetcher::~StatusFetcher() = default;

void StatusFetcher::OnError(const ProtoFetcherStatus& status) {
  OnStatus(status);
}
void StatusFetcher::OnResponse(std::unique_ptr<std::string> response_body) {
  OnStatus(ProtoFetcherStatus::Ok());
}
void StatusFetcher::OnStatus(const ProtoFetcherStatus& status) {
  RecordMetrics(status);
  std::move(callback_).Run(status);
}

std::unique_ptr<ClassifyUrlFetcher> CreateClassifyURLFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::ClassifyUrlRequest& request,
    const FetcherConfig& config,
    version_info::Channel channel) {
  return CreateFetcher<kidsmanagement::ClassifyUrlResponse>(
      identity_manager, url_loader_factory, request, config, /*args=*/{},
      channel);
}

std::unique_ptr<ListFamilyMembersFetcher> FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ListFamilyMembersFetcher::Callback callback,
    const FetcherConfig& config) {
  kidsmanagement::ListMembersRequest request;
  // If there is no associated Family Group with the account return an empty
  // response instead of NOT_FOUND.
  request.set_allow_empty_family(true);
  std::unique_ptr<ListFamilyMembersFetcher> fetcher =
      CreateFetcher<kidsmanagement::ListMembersResponse>(
          identity_manager, url_loader_factory, request, config);
  fetcher->Start(std::move(callback));
  return fetcher;
}

std::unique_ptr<PermissionRequestFetcher> CreatePermissionRequestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::PermissionRequest& request,
    const FetcherConfig& config) {
  return CreateFetcher<kidsmanagement::CreatePermissionRequestResponse>(
      identity_manager, url_loader_factory, request, config);
}

std::unique_ptr<ProtoFetcher<Response>> CreateTestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const Request& request,
    const FetcherConfig& config) {
  return CreateFetcher<Response>(identity_manager, url_loader_factory, request,
                                 config, /*args=*/{},
                                 version_info::Channel::UNKNOWN);
}

}  // namespace supervised_user
