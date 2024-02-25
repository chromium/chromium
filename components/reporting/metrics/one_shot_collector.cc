// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/one_shot_collector.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

OneShotCollector::OneShotCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& setting_path,
    bool setting_enabled_default_value,
    ReportQueue::EnqueueCallback on_data_reported)
    : OneShotCollector(sampler,
                       metric_report_queue,
                       reporting_settings,
                       setting_path,
                       setting_enabled_default_value,
                       /*init_delay=*/base::TimeDelta(),
                       std::move(on_data_reported)) {}

OneShotCollector::OneShotCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& setting_path,
    bool setting_enabled_default_value,
    base::TimeDelta init_delay,
    ReportQueue::EnqueueCallback on_data_reported)
    : CollectorBase(sampler),
      metric_report_queue_(metric_report_queue),
      on_data_reported_(std::move(on_data_reported)) {
  reporting_controller_ = std::make_unique<MetricReportingController>(
      reporting_settings, setting_path, setting_enabled_default_value);
  if (init_delay.is_zero()) {
    SetReportingControllerCb();
    return;
  }
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OneShotCollector::SetReportingControllerCb,
                     weak_ptr_factory_.GetWeakPtr()),
      init_delay);
}

OneShotCollector::~OneShotCollector() = default;

void OneShotCollector::Collect(bool is_event_driven) {
  CheckOnSequence();
  if (!is_event_driven) {
    if (data_collected_) {
      return;
    }
    // TODO(b/260093529): Should this be set for event driven telemetry
    // collection as well?
    data_collected_ = true;
  }
  CollectorBase::Collect(is_event_driven);
}

bool OneShotCollector::CanCollect() const {
  return reporting_controller_->IsEnabled();
}

void OneShotCollector::OnMetricDataCollected(
    bool is_event_driven,
    std::optional<MetricData> metric_data) {
  CheckOnSequence();
  CHECK(is_event_driven || on_data_reported_);
  CHECK(metric_report_queue_);
  if (!metric_data.has_value()) {
    base::UmaHistogramExactLinear(
        OneShotCollector::kNoMetricDataMetricsName,
        static_cast<int>(metric_report_queue_->GetDestination()),
        Destination_ARRAYSIZE);
    return;
  }

  metric_data->set_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  if (is_event_driven) {
    CHECK(metric_data->has_telemetry_data());
    metric_data->mutable_telemetry_data()->set_is_event_driven(is_event_driven);
  }
  metric_report_queue_->Enqueue(
      std::move(metric_data.value()),
      is_event_driven ? base::DoNothing() : std::move(on_data_reported_));
}

void OneShotCollector::SetReportingControllerCb() {
  CheckOnSequence();

  reporting_controller_->SetSettingUpdateCb(
      base::BindRepeating(&OneShotCollector::Collect, base::Unretained(this),
                          /*is_event_driven=*/false));
}
}  // namespace reporting
