// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that sends DWA logs to a server.

#ifndef COMPONENTS_METRICS_DWA_DWA_REPORTING_SERVICE_H_
#define COMPONENTS_METRICS_DWA_DWA_REPORTING_SERVICE_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "components/metrics/reporting_service.h"
#include "components/metrics/unsent_log_store.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {
class MetricsServiceClient;

namespace dwa {

// A service that uploads logs to the DWA server.
class DwaReportingService : public metrics::ReportingService {
 public:
  // Creates the DwaReportingService with the given |client|, |local_state|, and
  // |storage_limits|. Does not take ownership of the parameters; instead stores
  // a weak pointer to each. Caller should ensure that the parameters are valid
  // for the lifetime of this class.
  DwaReportingService(
      metrics::MetricsServiceClient* client,
      PrefService* local_state,
      const UnsentLogStore::UnsentLogStoreLimits& storage_limits);

  DwaReportingService(const DwaReportingService&) = delete;
  DwaReportingService& operator=(const DwaReportingService&) = delete;

  ~DwaReportingService() override;

  // metrics::ReportingService:
  metrics::LogStore* log_store() override;

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // metrics::ReportingService:
  GURL GetUploadUrl() const override;
  GURL GetInsecureUploadUrl() const override;
  std::string_view upload_mime_type() const override;
  metrics::MetricsLogUploader::MetricServiceType service_type() const override;

  metrics::UnsentLogStore unsent_log_store_;
};

}  // namespace dwa
}  // namespace metrics

#endif  // COMPONENTS_METRICS_DWA_DWA_REPORTING_SERVICE_H_
