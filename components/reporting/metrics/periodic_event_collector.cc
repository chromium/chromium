// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/periodic_event_collector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

PeriodicEventCollector::PeriodicEventCollector(
    Sampler* sampler,
    std::unique_ptr<EventDetector> event_detector,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : CollectorBase(sampler),
      event_detector_(std::move(event_detector)),
      rate_controller_(std::make_unique<MetricRateController>(
          base::BindRepeating(&PeriodicEventCollector::Collect,
                              base::Unretained(this),
                              /*is_event_driven=*/false),
          reporting_settings,
          rate_setting_path,
          default_rate,
          rate_unit_to_ms)) {}

PeriodicEventCollector::~PeriodicEventCollector() = default;

void PeriodicEventCollector::SetOnEventObservedCallback(
    MetricRepeatingCallback cb) {
  CHECK(!on_event_observed_cb_);
  on_event_observed_cb_ = std::move(cb);
}

void PeriodicEventCollector::SetReportingEnabled(bool is_enabled) {
  if (is_enabled) {
    // Do initial collection at startup.
    Collect(/*is_event_driven=*/false);
    rate_controller_->Start();
    return;
  }
  rate_controller_->Stop();
}

void PeriodicEventCollector::OnMetricDataCollected(
    bool is_event_driven,
    std::optional<MetricData> metric_data) {
  if (!metric_data.has_value()) {
    return;
  }

  metric_data->set_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  std::optional<MetricEventType> event =
      event_detector_->DetectEvent(last_collected_data_, metric_data.value());
  last_collected_data_ = std::move(metric_data.value());

  if (!on_event_observed_cb_ || !event.has_value()) {
    return;
  }
  last_collected_data_->mutable_event_data()->set_type(event.value());
  on_event_observed_cb_.Run(last_collected_data_.value());
}

bool PeriodicEventCollector::CanCollect() const {
  return true;
}
}  // namespace reporting
