// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService specialized to report UMA metrics.

#include "components/metrics/metrics_reporting_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics {

namespace {

// If an upload fails, and the transmission was over this byte count, then we
// will discard the log, and not try to retransmit it.  We also don't persist
// the log to the prefs for transmission during the next chrome session if this
// limit is exceeded.
const size_t kUploadLogAvoidRetransmitSize = 100 * 1024;

}  // namespace

// static
void MetricsReportingService::RegisterPrefs(PrefRegistrySimple* registry) {
  ReportingService::RegisterPrefs(registry);
  MetricsLogStore::RegisterPrefs(registry);
}

MetricsReportingService::MetricsReportingService(MetricsServiceClient* client,
                                                 PrefService* local_state)
    : ReportingService(client, local_state, kUploadLogAvoidRetransmitSize),
      metrics_log_store_(local_state,
                         kUploadLogAvoidRetransmitSize,
                         client->GetUploadSigningKey()) {}

MetricsReportingService::~MetricsReportingService() {}

LogStore* MetricsReportingService::log_store() {
  return &metrics_log_store_;
}

GURL MetricsReportingService::GetUploadUrl() const {
  return client()->GetMetricsServerUrl();
}

GURL MetricsReportingService::GetInsecureUploadUrl() const {
  return client()->GetInsecureMetricsServerUrl();
}

base::StringPiece MetricsReportingService::upload_mime_type() const {
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
                              base::TimeDelta::FromHours(12).InMinutes(), 50);
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

void MetricsReportingService::LogSuccess(size_t log_size) {
  UMA_HISTOGRAM_COUNTS_10000("UMA.LogSize.OnSuccess", log_size / 1024);
}

void MetricsReportingService::LogLargeRejection(size_t log_size) {
  UMA_HISTOGRAM_COUNTS_1M("UMA.Large Rejected Log was Discarded",
                          static_cast<int>(log_size));
}

}  // namespace metrics
