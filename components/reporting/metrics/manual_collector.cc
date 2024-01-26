// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/manual_collector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

ManualCollector::ManualCollector(Sampler* sampler,
                                 MetricReportQueue* metric_report_queue,
                                 ReportingSettings* reporting_settings,
                                 const std::string& setting_path,
                                 bool setting_enabled_default_value)
    : CollectorBase(sampler),
      metric_report_queue_(metric_report_queue),
      reporting_controller_(std::make_unique<MetricReportingController>(
          reporting_settings,
          setting_path,
          setting_enabled_default_value)) {}

ManualCollector::~ManualCollector() = default;

bool ManualCollector::CanCollect() const {
  return reporting_controller_->IsEnabled();
}

void ManualCollector::OnMetricDataCollected(
    bool is_event_driven,
    std::optional<MetricData> metric_data) {
  CheckOnSequence();
  CHECK(metric_report_queue_);
  if (!metric_data.has_value()) {
    base::UmaHistogramExactLinear(
        ManualCollector::kNoMetricDataMetricsName,
        static_cast<int>(metric_report_queue_->GetDestination()),
        Destination_ARRAYSIZE);
    return;
  }
  if (is_event_driven) {
    CHECK(metric_data->has_telemetry_data());
    metric_data->mutable_telemetry_data()->set_is_event_driven(is_event_driven);
  }
  metric_data->set_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  metric_report_queue_->Enqueue(std::move(metric_data.value()));
}
}  // namespace reporting
