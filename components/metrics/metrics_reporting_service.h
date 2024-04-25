// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that sends metrics logs to a server.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_SERVICE_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "components/metrics/metrics_log_store.h"
#include "components/metrics/reporting_service.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class MetricsServiceClient;

// MetricsReportingService is concrete implementation of ReportingService for
// UMA logs. It uses a MetricsLogStore as its LogStore, reports to the UMA
// endpoint, and logs some histograms with the UMA prefix.
class MetricsReportingService : public ReportingService {
 public:
  // Creates a ReportingService with the given |client|, |local_state|, and
  // |logs_event_manager_|. Does not take ownership of the parameters; instead
  // it stores a weak pointer to each. Caller should ensure that the parameters
  // are valid for the lifetime of this class. |logs_event_manager| is used to
  // notify observers of log events. Can be set to null if observing the events
  // is not necessary.
  MetricsReportingService(MetricsServiceClient* client,
                          PrefService* local_state,
                          MetricsLogsEventManager* logs_event_manager_);

  MetricsReportingService(const MetricsReportingService&) = delete;
  MetricsReportingService& operator=(const MetricsReportingService&) = delete;

  ~MetricsReportingService() override;

  MetricsLogStore* metrics_log_store() { return &metrics_log_store_; }
  const MetricsLogStore* metrics_log_store() const {
    return &metrics_log_store_;
  }

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // ReportingService:
  LogStore* log_store() override;
  GURL GetUploadUrl() const override;
  GURL GetInsecureUploadUrl() const override;
  std::string_view upload_mime_type() const override;
  MetricsLogUploader::MetricServiceType service_type() const override;
  void LogActualUploadInterval(base::TimeDelta interval) override;
  void LogCellularConstraint(bool upload_canceled) override;
  void LogResponseOrErrorCode(int response_code,
                              int error_code,
                              bool was_https) override;
  void LogSuccessLogSize(size_t log_size) override;
  void LogSuccessMetadata(const std::string& staged_log) override;
  void LogLargeRejection(size_t log_size) override;

  MetricsLogStore metrics_log_store_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_SERVICE_H_
