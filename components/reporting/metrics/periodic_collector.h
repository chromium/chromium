// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_PERIODIC_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_PERIODIC_COLLECTOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

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
  // Metrics name for reporting missing metric data to UMA.
  static constexpr char kNoMetricDataMetricsName[] =
      "Browser.ERP.MetricsReporting.PeriodicCollectorNoMetricData";

  // Start periodic collection after `init_delay`.
  PeriodicCollector(Sampler* sampler,
                    MetricReportQueue* metric_report_queue,
                    ReportingSettings* reporting_settings,
                    const std::string& enable_setting_path,
                    bool setting_enabled_default_value,
                    const std::string& rate_setting_path,
                    base::TimeDelta default_rate,
                    int rate_unit_to_ms,
                    base::TimeDelta init_delay);

  // Start periodic collection immediately.
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
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             std::optional<MetricData> metric_data) override;
  bool CanCollect() const override;

 private:
  void StartPeriodicCollection();

  void StopPeriodicCollection();

  void SetReportingControllerCb();

  const raw_ptr<MetricReportQueue> metric_report_queue_;

  // `rate_controller_` should be initialized before the setting update
  // callbacks of `reporting_controller_` are set, as `reporting_controller`
  // will trigger `rate_controller_` call if the setting is enabled.
  const std::unique_ptr<MetricRateController> rate_controller_;
  const std::unique_ptr<MetricReportingController> reporting_controller_;

  base::WeakPtrFactory<PeriodicCollector> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_PERIODIC_COLLECTOR_H_
