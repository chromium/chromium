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
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/request_priority.h"
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
// buffer message and a system parameter that is configurable per Request type.
GURL CreateRequestUrl(const FetcherConfig& config,
                      const FetcherConfig::PathArgs& args) {
  CHECK(!config.service_endpoint.Get().empty())
      << "Service endpoint is required";

  if (config.method == FetcherConfig::Method::kGet) {
    std::string url =
        base::StrCat({config.ServicePath(args), "?", kSystemParameters});
    if (!config.system_param_suffix.empty()) {
      url += base::StrCat({"&", config.system_param_suffix});
    }
    return GURL(config.service_endpoint.Get()).Resolve(url);
  }

  CHECK(config.system_param_suffix.empty())
      << "System param suffix support for GET requests only.";
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
    google_apis::AddDefaultAPIKeyToRequest(*resource_request, *channel);
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
                          elapsed_timer_.Elapsed());
}

void Metrics::RecordStatusLatency(const ProtoFetcherStatus& status) const {
  base::UmaHistogramTimes(GetFullHistogramName(MetricType::kLatency, status),
                          elapsed_timer_.Elapsed());
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
    case MetricType::kAuthError:
      return "AuthError";
    case MetricType::kRetryCount:
      NOTREACHED();
    default:
      NOTREACHED();
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
    case ProtoFetcherStatus::State::OK:
      return "NoError";
    case ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR:
      return "AuthError";
    case ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR:
      return "HttpStatusOrNetError";
    case ProtoFetcherStatus::State::INVALID_RESPONSE:
      return "ParseError";
    case ProtoFetcherStatus::State::DATA_ERROR:
      return "DataError";
    default:
      NOTREACHED();
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
      NOTREACHED();
    case MetricType::kRetryCount:
      return "RetryCount";
    default:
      NOTREACHED();
  }
}

void OverallMetrics::RecordRetryCount(int count) const {
  // It's a prediction that it will take less than 100 retries to get a
  // decisive response. Double exponential backoff set at 4 hour limit
  // shouldn't exhaust this limit too soon.
  base::UmaHistogramCounts100(GetFullHistogramName(MetricType::kRetryCount),
                              count);
}

FetchProcess::FetchProcess(
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
                   &FetchProcess::OnAccessTokenFetchComplete,
                   base::Unretained(this),  // Unretained(.) is safe because
                                            // `this` owns `fetcher_`.
                   url_loader_factory)) {}
FetchProcess::~FetchProcess() = default;
bool FetchProcess::IsMetricsRecordingEnabled() const {
  return metrics_.has_value();
}

void FetchProcess::RecordMetrics(const ProtoFetcherStatus& status) {
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

void FetchProcess::OnAccessTokenFetchComplete(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
        access_token) {
  if (!access_token.has_value()) {
    access_token_auth_error_ = access_token.error();
    if (config_->access_token_config.credentials_requirement ==
        AccessTokenConfig::CredentialsRequirement::kStrict) {
      OnError(ProtoFetcherStatus::GoogleServiceAuthError(access_token.error()));
      return;
    }
  }

  simple_url_loader_ = InitializeSimpleUrlLoader(
      base::OptionalFromExpected(access_token), config_.get(), args_, channel_,
      GetRequestPayload());
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(
          &FetchProcess::OnSimpleUrlLoaderComplete,
          base::Unretained(this)));  // Unretained(.) is safe because
                                     // `this` owns `simple_url_loader_`.
}

void FetchProcess::OnSimpleUrlLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!IsLoadingSuccessful(*simple_url_loader_) ||
      !HasHttpOkResponse(*simple_url_loader_)) {
    OnError(ProtoFetcherStatus::HttpStatusOrNetError(
        HttpStatusOrNetError(*simple_url_loader_)));
    return;
  }

  OnResponse(std::move(response_body));
}

std::optional<std::string> FetchProcess::GetRequestPayload() const {
  if (config_->method == FetcherConfig::Method::kGet) {
    CHECK(payload_.empty());
    return std::nullopt;
  }
  return payload_;
}
}  // namespace supervised_user
