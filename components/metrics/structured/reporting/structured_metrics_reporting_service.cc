// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/structured/reporting/structured_metrics_log_metrics.h"
#include "components/metrics/structured/structured_metrics_prefs.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::structured::reporting {
StructuredMetricsReportingService::StructuredMetricsReportingService(
    MetricsServiceClient* client,
    PrefService* local_state,
    const UnsentLogStore::UnsentLogStoreLimits& storage_limits)
    : ReportingService(client,
                       local_state,
                       storage_limits.max_log_size_bytes,
                       /*logs_event_manager=*/nullptr),
      log_store_(std::make_unique<StructuredMetricsLogMetrics>(),
                 local_state,
                 prefs::kLogStoreName,
                 /* metadata_pref_name=*/nullptr,
                 storage_limits,
                 client->GetUploadSigningKey(),
                 /* logs_event_manager=*/nullptr) {}

void StructuredMetricsReportingService::StoreLog(
    const std::string& serialized_log,
    metrics::MetricsLogsEventManager::CreateReason reason) {
  LogMetadata metadata;
  log_store_.StoreLog(serialized_log, metadata, reason);
}

metrics::LogStore* StructuredMetricsReportingService::log_store() {
  return &log_store_;
}

void StructuredMetricsReportingService::Purge() {
  log_store_.Purge();
}

// Getters for MetricsLogUploader parameters.
GURL StructuredMetricsReportingService::GetUploadUrl() const {
  return client()->GetMetricsServerUrl();
}
GURL StructuredMetricsReportingService::GetInsecureUploadUrl() const {
  return client()->GetInsecureMetricsServerUrl();
}

std::string_view StructuredMetricsReportingService::upload_mime_type() const {
  return kDefaultMetricsMimeType;
}

MetricsLogUploader::MetricServiceType
StructuredMetricsReportingService::service_type() const {
  return MetricsLogUploader::STRUCTURED_METRICS;
}

// Methods for recording data to histograms.
void StructuredMetricsReportingService::LogActualUploadInterval(
    base::TimeDelta interval) {
  base::UmaHistogramCustomCounts(
      "StructuredMetrics.Reporting.ActualUploadInterval", interval.InMinutes(),
      1, base::Hours(12).InMinutes(), 50);
}

void StructuredMetricsReportingService::LogResponseOrErrorCode(
    int response_code,
    int error_code,
    bool /*was_http*/) {
  // TODO(crbug.com/40268040) Do we assume |was_https| is always true? UMA
  // doesn't but UKM does.
  if (response_code >= 0) {
    base::UmaHistogramSparse("StructuredMetrics.Reporting.HTTPResponseCode",
                             response_code);
  } else {
    base::UmaHistogramSparse("StructuredMetrics.Reporting.HTTPErrorCode",
                             error_code);
  }
}

void StructuredMetricsReportingService::LogSuccessLogSize(size_t log_size) {
  base::UmaHistogramMemoryKB("StructuredMetrics.Reporting.LogSize.OnSuccess",
                             log_size);
}

void StructuredMetricsReportingService::LogLargeRejection(size_t log_size) {
  base::UmaHistogramMemoryKB("StructuredMetrics.Reporting.LogSize.RejectedSize",
                             log_size);
}

// static:
void StructuredMetricsReportingService::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kLogStoreName);
}

}  // namespace metrics::structured::reporting
