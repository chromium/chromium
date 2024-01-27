// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_ONE_SHOT_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_ONE_SHOT_COLLECTOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

class MetricReportQueue;
class MetricReportingController;
class ReportingSettings;

// Class to collect and report metric data only one time when the reporting
// setting is enabled.
class OneShotCollector : public CollectorBase {
 public:
  // Metrics name for reporting missing metric data to UMA.
  static constexpr char kNoMetricDataMetricsName[] =
      "Browser.ERP.MetricsReporting.OneShotCollectorNoMetricData";

  // Start observing the reporting setting immediately to start collection.
  OneShotCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& setting_path,
      bool setting_enabled_default_value,
      ReportQueue::EnqueueCallback on_data_reported = base::DoNothing());

  // Start observing the reporting setting after `init_delay` to start
  // collection.
  OneShotCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& setting_path,
      bool setting_enabled_default_value,
      base::TimeDelta init_delay,
      ReportQueue::EnqueueCallback on_data_reported = base::DoNothing());

  OneShotCollector(const OneShotCollector& other) = delete;
  OneShotCollector& operator=(const OneShotCollector& other) = delete;

  ~OneShotCollector() override;

  // CollectorBase:
  void Collect(bool is_event_driven) override;

 protected:
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             std::optional<MetricData> metric_data) override;
  bool CanCollect() const override;

 private:
  void SetReportingControllerCb();

  const raw_ptr<MetricReportQueue> metric_report_queue_;

  std::unique_ptr<MetricReportingController> reporting_controller_;

  ReportQueue::EnqueueCallback on_data_reported_;

  bool data_collected_ = false;

  base::WeakPtrFactory<OneShotCollector> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_ONE_SHOT_COLLECTOR_H_
