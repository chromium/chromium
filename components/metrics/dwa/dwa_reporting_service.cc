// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService specialized to report DWA metrics.

#include "components/metrics/dwa/dwa_reporting_service.h"

#include <memory>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/dwa/dwa_unsent_log_store_metrics.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/server_urls.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::dwa {

DwaReportingService::DwaReportingService(
    metrics::MetricsServiceClient* client,
    PrefService* local_state,
    const UnsentLogStore::UnsentLogStoreLimits& storage_limits)
    : ReportingService(client,
                       local_state,
                       storage_limits.max_log_size_bytes,
                       /*logs_event_manager=*/nullptr),
      unsent_log_store_(std::make_unique<DwaUnsentLogStoreMetrics>(),
                        local_state,
                        prefs::kUnsentLogStoreName,
                        /*metadata_pref_name=*/nullptr,
                        storage_limits,
                        client->GetUploadSigningKey(),
                        /*logs_event_manager=*/nullptr) {}

DwaReportingService::~DwaReportingService() = default;

metrics::UnsentLogStore* DwaReportingService::unsent_log_store() {
  return &unsent_log_store_;
}

// static
void DwaReportingService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kUnsentLogStoreName);
}

metrics::LogStore* DwaReportingService::log_store() {
  return &unsent_log_store_;
}

GURL DwaReportingService::GetUploadUrl() const {
  return metrics::GetDwaServerUrl();
}

GURL DwaReportingService::GetInsecureUploadUrl() const {
  // Returns an empty string since retrying over HTTP is not enabled for DWA.
  return GURL();
}

std::string_view DwaReportingService::upload_mime_type() const {
  return kMetricsMimeType;
}

metrics::MetricsLogUploader::MetricServiceType
DwaReportingService::service_type() const {
  return MetricsLogUploader::DWA;
}

void DwaReportingService::LogCellularConstraint(bool upload_canceled) {
  UMA_HISTOGRAM_BOOLEAN("DWA.LogUpload.Canceled.CellularConstraint",
                        upload_canceled);
}

void DwaReportingService::LogResponseOrErrorCode(int response_code,
                                                 int error_code,
                                                 bool was_https) {
  // `was_https` is ignored since all DWA logs are received over HTTPS.
  base::UmaHistogramSparse("DWA.LogUpload.ResponseOrErrorCode",
                           response_code >= 0 ? response_code : error_code);
}

void DwaReportingService::LogSuccessLogSize(size_t log_size) {
  UMA_HISTOGRAM_COUNTS_10000("DWA.LogSize.OnSuccess", log_size / 1024);
}

void DwaReportingService::LogSuccessMetadata(const std::string& staged_log) {}

void DwaReportingService::LogLargeRejection(size_t log_size) {}

}  // namespace metrics::dwa
