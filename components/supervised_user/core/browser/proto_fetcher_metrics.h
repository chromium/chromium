// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_METRICS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_METRICS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/timer/elapsed_timer.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace supervised_user {
struct FetcherConfig;
class ProtoFetcherStatus;

// Encapsulates metric functionalities for proto fetchers.
class ProtoFetcherMetrics {
 public:
  ProtoFetcherMetrics() = delete;
  static std::optional<ProtoFetcherMetrics> FromConfig(
      const FetcherConfig& config);

  // Records metrics for the proto fetcher based on its configuration.
  void RecordMetrics(const ProtoFetcherStatus& status) const;

 protected:
  // The type of metrics tracking expected from the configuration.
  enum class MetricsTrackingMode : int {
    kSingle = 0,
    kOverall = 1,
  };

  // The metric type that is being recorded.
  enum class Type : int {
    kStatus = 1,
    kLatency = 2,
    kHttpStatusOrNetError = 3,
    kRetryCount = 4,
    kAuthError = 5,
  };

  ProtoFetcherMetrics(std::string_view basename,
                      MetricsTrackingMode metrics_tracking_mode);

  // Returns fully-qualified name of histogram for specified metric_type.
  std::string GetFullHistogramName(Type metric_type) const;

  // Returns label associated to the tracking mode.
  std::string GetMetricsTrackingModeLabel() const;

  MetricsTrackingMode metrics_tracking_mode_;

 private:
  // Records the ProtoFetcherStatus count.
  void RecordStatus(const ProtoFetcherStatus& status) const;

  // Records latency from the instantiation of the class (up to 10s).
  void RecordLatency() const;

  // Records latency per ProtoFetcherStatus type.
  void RecordStatusLatency(const ProtoFetcherStatus& status) const;

  // Records errors from ProtoFetcherStatus.
  void RecordAuthError(const GoogleServiceAuthError& auth_error) const;
  void RecordHttpStatusOrNetError(const ProtoFetcherStatus& status) const;

  // Translates top-level metric type into a string. ::ToMetricEnumLabel
  // translates statuses for per-status latency tracking.
  std::string GetMetricKey(Type metric_type) const;

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-status values.
  std::string GetFullHistogramName(Type metric_type,
                                   ProtoFetcherStatus status) const;

  // The returned value must match one of the labels in
  // chromium/src/tools/metrics/histograms/enums.xml://enum[@name='ProtoFetcherStatus'],
  // and should be reflected in tokens in histogram defined for this fetcher.
  // See example at
  // tools/metrics/histograms/metadata/signin/histograms.xml://histogram[@name='Signin.ListFamilyMembersRequest.{Status}.*']
  static std::string ToMetricEnumLabel(const ProtoFetcherStatus& status);

  // Note for this non-owning reference that the corresponding fetcher config
  // that owns this basename has static linkage.
  std::string_view basename_;
  base::ElapsedTimer elapsed_timer_;
};

// Encapsulates metrics functionality for cumulative latency and status tracking
// during the proto fetching lifecycle. This is only available for proto
// fetchers that support retrying policy.
class CumulativeProtoFetcherMetrics : public ProtoFetcherMetrics {
 public:
  CumulativeProtoFetcherMetrics() = delete;
  static std::optional<CumulativeProtoFetcherMetrics> FromConfig(
      const FetcherConfig& config);

  // Records the number of retries from transient errors (up to 100).
  void RecordRetryCount(int count) const;

 private:
  CumulativeProtoFetcherMetrics(std::string_view basename,
                                MetricsTrackingMode metrics_tracking_mode);
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_METRICS_H_
