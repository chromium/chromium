// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/structured/structured_metrics_prefs.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::structured::reporting {
StructuredMetricsReportingService::StructuredMetricsReportingService(
    MetricsServiceClient* client,
    PrefService* local_state,
    const StorageLimits& storage_limits)
    : ReportingService(client,
                       local_state,
                       storage_limits.max_log_size,
                       /*logs_event_manager=*/nullptr),
      log_store_(std::make_unique<metrics::UnsentLogStoreMetricsImpl>(),
                 local_state,
                 prefs::kLogStoreName,
                 /* metadata_pref_name=*/nullptr,
                 storage_limits.min_log_queue_count,
                 storage_limits.min_log_queue_size,
                 storage_limits.max_log_size,
                 client->GetUploadSigningKey(),
                 /* logs_event_manager=*/nullptr) {}

metrics::LogStore* StructuredMetricsReportingService::log_store() {
  return &log_store_;
}

// Getters for MetricsLogUploader parameters.
GURL StructuredMetricsReportingService::GetUploadUrl() const {
  return client()->GetMetricsServerUrl();
}
GURL StructuredMetricsReportingService::GetInsecureUploadUrl() const {
  return client()->GetInsecureMetricsServerUrl();
}

base::StringPiece StructuredMetricsReportingService::upload_mime_type() const {
  return kDefaultMetricsMimeType;
}

MetricsLogUploader::MetricServiceType
StructuredMetricsReportingService::service_type() const {
  // TODO(andrewbregger): change to a structured_metrics service type.
  return MetricsLogUploader::UMA;
}

// static:
void StructuredMetricsReportingService::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kLogStoreName);
}
}  // namespace metrics::structured::reporting
