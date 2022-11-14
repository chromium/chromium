// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_PERIODIC_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_PERIODIC_COLLECTOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

class MetricRateController;
class MetricReportQueue;
class MetricReportingController;
class ReportingSettings;
class Sampler;

// Class to collect and report metric data periodically if the reporting setting
// is enabled.
class PeriodicCollector : public CollectorBase {
 public:
  PeriodicCollector(Sampler* sampler,
                    MetricReportQueue* metric_report_queue,
                    ReportingSettings* reporting_settings,
                    const std::string& enable_setting_path,
                    bool setting_enabled_default_value,
                    const std::string& rate_setting_path,
                    base::TimeDelta default_rate,
                    int rate_unit_to_ms = 1);

  PeriodicCollector(const PeriodicCollector& other) = delete;
  PeriodicCollector& operator=(const PeriodicCollector& other) = delete;

  ~PeriodicCollector() override;

 protected:
  void OnMetricDataCollected(absl::optional<MetricData> metric_data) override;

 private:
  virtual void StartPeriodicCollection();

  virtual void StopPeriodicCollection();

  const raw_ptr<MetricReportQueue> metric_report_queue_;

  // `rate_controller_` should be initialized before `reporting_controller_` as
  // initializing `reporting_controller` will trigger `rate_controller_` call if
  // the setting is enabled.
  const std::unique_ptr<MetricRateController> rate_controller_;
  const std::unique_ptr<MetricReportingController> reporting_controller_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_PERIODIC_COLLECTOR_H_
