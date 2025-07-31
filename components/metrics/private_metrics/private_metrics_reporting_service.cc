// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService specialized to report private metrics.

#include "components/metrics/private_metrics/private_metrics_reporting_service.h"

#include <memory>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/private_metrics/private_metrics_unsent_log_store_metrics.h"
#include "components/metrics/server_urls.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::private_metrics {

PrivateMetricsReportingService::PrivateMetricsReportingService(
    metrics::MetricsServiceClient* client,
    PrefService* local_state,
    const UnsentLogStore::UnsentLogStoreLimits& storage_limits)
    : ReportingService(client,
                       local_state,
                       storage_limits.max_log_size_bytes,
                       /*logs_event_manager=*/nullptr),
      unsent_log_store_(std::make_unique<PrivateMetricsUnsentLogStoreMetrics>(),
                        local_state,
                        dwa::prefs::kUnsentLogStoreName,
                        /*metadata_pref_name=*/nullptr,
                        storage_limits,
                        client->GetUploadSigningKey(),
                        /*logs_event_manager=*/nullptr) {}

PrivateMetricsReportingService::~PrivateMetricsReportingService() = default;

metrics::UnsentLogStore* PrivateMetricsReportingService::unsent_log_store() {
  return &unsent_log_store_;
}

// static
void PrivateMetricsReportingService::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(dwa::prefs::kUnsentLogStoreName);
}

metrics::LogStore* PrivateMetricsReportingService::log_store() {
  return &unsent_log_store_;
}

GURL PrivateMetricsReportingService::GetUploadUrl() const {
  return metrics::GetDwaServerUrl();
}

GURL PrivateMetricsReportingService::GetInsecureUploadUrl() const {
  // Returns an empty string since retrying over HTTP is not enabled for Private
  // Metrics.
  return GURL();
}

std::string_view PrivateMetricsReportingService::upload_mime_type() const {
  return kMetricsMimeType;
}

metrics::MetricsLogUploader::MetricServiceType
PrivateMetricsReportingService::service_type() const {
  return MetricsLogUploader::DWA;
}

void PrivateMetricsReportingService::LogCellularConstraint(
    bool upload_canceled) {
  base::UmaHistogramBoolean("DWA.LogUpload.Canceled.CellularConstraint",
                            upload_canceled);
}

void PrivateMetricsReportingService::LogResponseOrErrorCode(int response_code,
                                                            int error_code,
                                                            bool was_https) {
  // `was_https` is ignored since all Private Metrics logs are received over
  // HTTPS.
  base::UmaHistogramSparse("DWA.LogUpload.ResponseOrErrorCode",
                           response_code >= 0 ? response_code : error_code);
}

void PrivateMetricsReportingService::LogSuccessLogSize(size_t log_size) {
  base::UmaHistogramCounts10000("DWA.LogSize.OnSuccess", log_size / 1024);
}

void PrivateMetricsReportingService::LogSuccessMetadata(
    const std::string& staged_log) {}

void PrivateMetricsReportingService::LogLargeRejection(size_t log_size) {}

}  // namespace metrics::private_metrics
