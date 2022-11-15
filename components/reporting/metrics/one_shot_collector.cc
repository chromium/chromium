// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/one_shot_collector.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

void OneShotCollector::Collect() {
  CheckOnSequence();

  if (data_collected_) {
    return;
  }
  data_collected_ = true;
  reporting_controller_.reset();
  CollectorBase::Collect();
}

void OneShotCollector::OnMetricDataCollected(
    absl::optional<MetricData> metric_data) {
  CheckOnSequence();
  DCHECK(on_data_reported_);
  if (!metric_data.has_value()) {
    return;
  }

  metric_data->set_timestamp_ms(base::Time::Now().ToJavaTime());
  metric_report_queue_->Enqueue(std::move(metric_data.value()),
                                std::move(on_data_reported_));
}

void OneShotCollector::SetReportingControllerCb() {
  CheckOnSequence();

  reporting_controller_->SetSettingUpdateCb(
      base::BindRepeating(&OneShotCollector::Collect, base::Unretained(this)));
}
}  // namespace reporting
