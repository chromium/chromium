// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that sends ukm logs to a server.

#ifndef COMPONENTS_UKM_UKM_REPORTING_SERVICE_H_
#define COMPONENTS_UKM_UKM_REPORTING_SERVICE_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "components/metrics/reporting_service.h"
#include "components/metrics/unsent_log_store.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {
class MetricsServiceClient;
}

namespace ukm {

// A service that uploads logs to the UKM server.
class UkmReportingService : public metrics::ReportingService {
 public:
  // Creates the UkmReportingService with the given |client|, and
  // |local_state|.  Does not take ownership of the paramaters; instead stores
  // a weak pointer to each. Caller should ensure that the parameters are valid
  // for the lifetime of this class.
  UkmReportingService(metrics::MetricsServiceClient* client,
                      PrefService* local_state);

  UkmReportingService(const UkmReportingService&) = delete;
  UkmReportingService& operator=(const UkmReportingService&) = delete;

  ~UkmReportingService() override;

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  metrics::UnsentLogStore* ukm_log_store() { return &unsent_log_store_; }
  const metrics::UnsentLogStore* ukm_log_store() const {
    return &unsent_log_store_;
  }

 private:
  // metrics:ReportingService:
  metrics::LogStore* log_store() override;
  GURL GetUploadUrl() const override;
  // Returns an empty string since retrying over HTTP is not enabled for UKM
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

  metrics::UnsentLogStore unsent_log_store_;
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_REPORTING_SERVICE_H_
