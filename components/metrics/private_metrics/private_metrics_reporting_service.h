// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that sends private metrics logs to a server.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_REPORTING_SERVICE_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_REPORTING_SERVICE_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "components/metrics/reporting_service.h"
#include "components/metrics/unsent_log_store.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {
class MetricsServiceClient;

namespace private_metrics {

// A service that uploads logs to the private metrics server.
class PrivateMetricsReportingService : public metrics::ReportingService {
 public:
  // Creates the PrivateMetricsReportingService with the given `client`,
  // `local_state`, and `storage_limits`. Does not take ownership of the
  // parameters; instead stores a weak pointer to each. Caller should ensure
  // that the parameters are valid for the lifetime of this class.
  //
  // `dwa_compatibility` specifies whether the service should work in
  // compatibility mode with the old DWA implementation. If `nullopt`, the value
  // will be based on currently enabled flags.
  PrivateMetricsReportingService(
      metrics::MetricsServiceClient* client,
      PrefService* local_state,
      const UnsentLogStore::UnsentLogStoreLimits& storage_limits,
      std::optional<bool> dwa_compatibility = std::nullopt);

  PrivateMetricsReportingService(const PrivateMetricsReportingService&) =
      delete;
  PrivateMetricsReportingService& operator=(
      const PrivateMetricsReportingService&) = delete;

  ~PrivateMetricsReportingService() override;

  metrics::UnsentLogStore* unsent_log_store();

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // metrics::ReportingService:
  metrics::LogStore* log_store() override;
  GURL GetUploadUrl() const override;
  GURL GetInsecureUploadUrl() const override;
  std::string_view upload_mime_type() const override;
  metrics::MetricsLogUploader::MetricServiceType service_type() const override;
  void LogCellularConstraint(bool upload_canceled) override;
  void LogResponseOrErrorCode(int response_code,
                              int error_code,
                              bool was_https) override;
  void LogSuccessLogSize(size_t log_size) override;
  void LogSuccessMetadata(const std::string& staged_log) override;
  void LogLargeRejection(size_t log_size) override;

  // If true, this service works in a compatibility mode with old DWA
  // implementation.
  const bool dwa_compatibility_;

  metrics::UnsentLogStore unsent_log_store_;
};

}  // namespace private_metrics
}  // namespace metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_REPORTING_SERVICE_H_
