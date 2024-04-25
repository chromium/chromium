// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_REPORTING_STRUCTURED_METRICS_REPORTING_SERVICE_H_
#define COMPONENTS_METRICS_STRUCTURED_REPORTING_STRUCTURED_METRICS_REPORTING_SERVICE_H_

#include <string_view>

#include "components/metrics/reporting_service.h"
#include "components/metrics/unsent_log_store.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {
class MetricsServiceClient;
}

namespace metrics::structured::reporting {

// A service that uploads Structured Metrics logs to the UMA server.
class StructuredMetricsReportingService : public metrics::ReportingService {
 public:
  StructuredMetricsReportingService(
      MetricsServiceClient* client,
      PrefService* local_state,
      const UnsentLogStore::UnsentLogStoreLimits& storage_limits);

  void StoreLog(const std::string& serialized_log,
                metrics::MetricsLogsEventManager::CreateReason reason);

  // metrics::ReportingService:
  metrics::LogStore* log_store() override;

  void Purge();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Getters for MetricsLogUploader parameters.
  GURL GetUploadUrl() const override;
  GURL GetInsecureUploadUrl() const override;
  std::string_view upload_mime_type() const override;
  MetricsLogUploader::MetricServiceType service_type() const override;

  // Methods for submitting UMA histograms.
  void LogActualUploadInterval(base::TimeDelta interval) override;
  void LogResponseOrErrorCode(int response_code,
                              int error_code,
                              bool was_https) override;
  void LogSuccessLogSize(size_t log_size) override;
  void LogLargeRejection(size_t log_size) override;

  metrics::UnsentLogStore log_store_;
};
}  // namespace metrics::structured::reporting

#endif  // COMPONENTS_METRICS_STRUCTURED_REPORTING_STRUCTURED_METRICS_REPORTING_SERVICE_H_
