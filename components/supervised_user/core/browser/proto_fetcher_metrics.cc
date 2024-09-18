// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher_metrics.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"

namespace supervised_user {
namespace {

bool IsRetryPolicyEnabled(const FetcherConfig& config) {
  return config.backoff_policy.has_value();
}

}  // namespace

std::optional<CumulativeProtoFetcherMetrics>
CumulativeProtoFetcherMetrics::FromConfig(const FetcherConfig& config) {
  if (IsRetryPolicyEnabled(config) && config.histogram_basename.has_value()) {
    return CumulativeProtoFetcherMetrics(*config.histogram_basename,
                                         MetricsTrackingMode::kOverall);
  }
  return std::nullopt;
}

CumulativeProtoFetcherMetrics::CumulativeProtoFetcherMetrics(
    std::string_view basename,
    MetricsTrackingMode metrics_tracking_mode)
    : ProtoFetcherMetrics(basename, metrics_tracking_mode) {}

void CumulativeProtoFetcherMetrics::RecordRetryCount(int count) const {
  CHECK(metrics_tracking_mode_ == MetricsTrackingMode::kOverall);
  // It's a prediction that it will take less than 100 retries to get a
  // decisive response. Double exponential backoff set at 4 hour limit
  // shouldn't exhaust this limit too soon.
  base::UmaHistogramCounts100(GetFullHistogramName(Type::kRetryCount), count);
}

ProtoFetcherMetrics::ProtoFetcherMetrics(
    std::string_view basename,
    MetricsTrackingMode metrics_tracking_mode)
    : metrics_tracking_mode_(metrics_tracking_mode), basename_(basename) {}

// static
std::optional<ProtoFetcherMetrics> ProtoFetcherMetrics::FromConfig(
    const FetcherConfig& config) {
  if (config.histogram_basename.has_value()) {
    return ProtoFetcherMetrics(*config.histogram_basename,
                               MetricsTrackingMode::kSingle);
  }
  return std::nullopt;
}

void ProtoFetcherMetrics::RecordMetrics(
    const ProtoFetcherStatus& status) const {
  RecordStatus(status);
  RecordLatency();
  if (metrics_tracking_mode_ == MetricsTrackingMode::kOverall) {
    return;
  }

  RecordStatusLatency(status);

  if (status.state() == ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR) {
    RecordAuthError(status.google_service_auth_error());
  }
  if (status.state() == ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR) {
    RecordHttpStatusOrNetError(status);
  }
}

void ProtoFetcherMetrics::RecordStatus(const ProtoFetcherStatus& status) const {
  base::UmaHistogramEnumeration(GetFullHistogramName(Type::kStatus),
                                status.state());
}

void ProtoFetcherMetrics::RecordLatency() const {
  base::UmaHistogramTimes(GetFullHistogramName(Type::kLatency),
                          elapsed_timer_.Elapsed());
}

void ProtoFetcherMetrics::RecordStatusLatency(
    const ProtoFetcherStatus& status) const {
  CHECK(metrics_tracking_mode_ == MetricsTrackingMode::kSingle)
      << "Status latency metric not supported for overall metrics";
  base::UmaHistogramTimes(GetFullHistogramName(Type::kLatency, status),
                          elapsed_timer_.Elapsed());
}

void ProtoFetcherMetrics::RecordAuthError(
    const GoogleServiceAuthError& auth_error) const {
  base::UmaHistogramEnumeration(GetFullHistogramName(Type::kAuthError),
                                auth_error.state(),
                                GoogleServiceAuthError::NUM_STATES);
}

void ProtoFetcherMetrics::RecordHttpStatusOrNetError(
    const ProtoFetcherStatus& status) const {
  CHECK_EQ(status.state(), ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  base::UmaHistogramSparse(GetFullHistogramName(Type::kHttpStatusOrNetError),
                           status.http_status_or_net_error().value());
}

std::string ProtoFetcherMetrics::GetMetricsTrackingModeLabel() const {
  switch (metrics_tracking_mode_) {
    case MetricsTrackingMode::kOverall:
      return "Overall";
    case MetricsTrackingMode::kSingle:
      return "";
  }
}

std::string ProtoFetcherMetrics::GetMetricKey(Type metric_type) const {
  switch (metric_type) {
    case Type::kStatus:
      return base::StrCat({GetMetricsTrackingModeLabel(), "Status"});
    case Type::kLatency:
      return base::StrCat({GetMetricsTrackingModeLabel(), "Latency"});
    case Type::kHttpStatusOrNetError:
      CHECK(metrics_tracking_mode_ == MetricsTrackingMode::kSingle);
      return "HttpStatusOrNetError";
    case Type::kAuthError:
      CHECK(metrics_tracking_mode_ == MetricsTrackingMode::kSingle);
      return "AuthError";
    case Type::kRetryCount:
      CHECK(metrics_tracking_mode_ == MetricsTrackingMode::kOverall);
      return "RetryCount";
    default:
      NOTREACHED();
  }
}

std::string ProtoFetcherMetrics::GetFullHistogramName(Type metric_type) const {
  return base::JoinString({basename_, GetMetricKey(metric_type)}, ".");
}

std::string ProtoFetcherMetrics::GetFullHistogramName(
    Type metric_type,
    ProtoFetcherStatus status) const {
  return base::JoinString(
      {basename_, ToMetricEnumLabel(status), GetMetricKey(metric_type)}, ".");
}

std::string ProtoFetcherMetrics::ToMetricEnumLabel(
    const ProtoFetcherStatus& status) {
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

}  // namespace supervised_user
