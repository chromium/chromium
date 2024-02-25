// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_MANUAL_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_MANUAL_COLLECTOR_H_

#include <memory>
#include <string>

#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

class MetricReportQueue;
class MetricReportingController;
class ReportingSettings;

// Class to collect and report metric data when manually triggered. Does not
// automatically collect data. Necessary reporting settings must be set or
// `setting_enabled_default_value` must be set to True in order to trigger
// collection.
class ManualCollector : public CollectorBase {
 public:
  // Metrics name for reporting missing metric data to UMA.
  static constexpr char kNoMetricDataMetricsName[] =
      "Browser.ERP.MetricsReporting.ManualCollectorNoMetricData";

  ManualCollector(Sampler* sampler,
                  MetricReportQueue* metric_report_queue,
                  ReportingSettings* reporting_settings,
                  const std::string& setting_path,
                  bool setting_enabled_default_value);

  ManualCollector(const ManualCollector& other) = delete;
  ManualCollector& operator=(const ManualCollector& other) = delete;
  ~ManualCollector() override;

 protected:
  // CollectorBase:
  bool CanCollect() const override;
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             std::optional<MetricData> metric_data) override;

 private:
  const raw_ptr<MetricReportQueue> metric_report_queue_;
  const std::unique_ptr<MetricReportingController> reporting_controller_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_MANUAL_COLLECTOR_H_
