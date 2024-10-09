// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService specialized to report UMA metrics.

#include "components/metrics/metrics_reporting_service.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics {

// static
void MetricsReportingService::RegisterPrefs(PrefRegistrySimple* registry) {
  ReportingService::RegisterPrefs(registry);
  MetricsLogStore::RegisterPrefs(registry);
}

MetricsReportingService::MetricsReportingService(
    MetricsServiceClient* client,
    PrefService* local_state,
    MetricsLogsEventManager* logs_event_manager_)
    : ReportingService(client,
                       local_state,
                       client->GetStorageLimits()
                           .ongoing_log_queue_limits.max_log_size_bytes,
                       logs_event_manager_),
      metrics_log_store_(local_state,
                         client->GetStorageLimits(),
                         client->GetUploadSigningKey(),
                         logs_event_manager_) {}

MetricsReportingService::~MetricsReportingService() = default;

LogStore* MetricsReportingService::log_store() {
  return &metrics_log_store_;
}

GURL MetricsReportingService::GetUploadUrl() const {
  return client()->GetMetricsServerUrl();
}

GURL MetricsReportingService::GetInsecureUploadUrl() const {
  return client()->GetInsecureMetricsServerUrl();
}

std::string_view MetricsReportingService::upload_mime_type() const {
  return kDefaultMetricsMimeType;
}

MetricsLogUploader::MetricServiceType MetricsReportingService::service_type()
    const {
  return MetricsLogUploader::UMA;
}

void MetricsReportingService::LogActualUploadInterval(
    base::TimeDelta interval) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("UMA.ActualLogUploadInterval",
                              interval.InMinutes(), 1,
                              base::Hours(12).InMinutes(), 50);
}

void MetricsReportingService::LogCellularConstraint(bool upload_canceled) {
  UMA_HISTOGRAM_BOOLEAN("UMA.LogUpload.Canceled.CellularConstraint",
                        upload_canceled);
}

void MetricsReportingService::LogResponseOrErrorCode(int response_code,
                                                     int error_code,
                                                     bool was_https) {
  if (was_https) {
    base::UmaHistogramSparse("UMA.LogUpload.ResponseOrErrorCode",
                             response_code >= 0 ? response_code : error_code);
  } else {
    base::UmaHistogramSparse("UMA.LogUpload.ResponseOrErrorCode.HTTP",
                             response_code >= 0 ? response_code : error_code);
  }
}

void MetricsReportingService::LogSuccessLogSize(size_t log_size) {
  UMA_HISTOGRAM_COUNTS_10000("UMA.LogSize.OnSuccess", log_size / 1024);
}

void MetricsReportingService::LogSuccessMetadata(
    const std::string& staged_log) {}

void MetricsReportingService::LogLargeRejection(size_t log_size) {}

}  // namespace metrics
