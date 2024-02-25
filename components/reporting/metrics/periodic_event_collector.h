// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_PERIODIC_EVENT_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_PERIODIC_EVENT_COLLECTOR_H_

#include <memory>
#include <optional>
#include <string>

#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace base {

class TimeDelta;

}  // namespace base

namespace reporting {

class MetricRateController;
class ReportingSettings;
class Sampler;

// Class to periodically collect telemetry data, check the collected telemetry
// data for events using `PeriodicEventCollector::EventDetector`, and notify the
// subscriber that subscribed using `SetOnEventObservedCallback` if reporting is
// enabled.
class PeriodicEventCollector : public MetricEventObserver,
                               public CollectorBase {
 public:
  // Interface for events detection in collected metric data.
  class EventDetector {
   public:
    virtual ~EventDetector() = default;
    // Check if there is a new event present in `current_metric_data` and return
    // it if found, `previous_metric_data` will be nullopt for first metric
    // collection.
    virtual std::optional<MetricEventType> DetectEvent(
        std::optional<MetricData> previous_metric_data,
        const MetricData& current_metric_data) = 0;
  };

  PeriodicEventCollector(Sampler* sampler,
                         std::unique_ptr<EventDetector> event_detector,
                         ReportingSettings* reporting_settings,
                         const std::string& rate_setting_path,
                         base::TimeDelta default_rate,
                         int rate_unit_to_ms = 1);

  PeriodicEventCollector(const PeriodicEventCollector&) = delete;
  PeriodicEventCollector operator=(const PeriodicEventCollector&) = delete;

  ~PeriodicEventCollector() override;

  // MetricEventObserver:
  void SetOnEventObservedCallback(MetricRepeatingCallback cb) override;
  void SetReportingEnabled(bool is_enabled) override;

 protected:
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             std::optional<MetricData> metric_data) override;
  bool CanCollect() const override;

 private:
  MetricRepeatingCallback on_event_observed_cb_;

  const std::unique_ptr<EventDetector> event_detector_;

  std::optional<MetricData> last_collected_data_;

  const std::unique_ptr<MetricRateController> rate_controller_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_PERIODIC_EVENT_COLLECTOR_H_
