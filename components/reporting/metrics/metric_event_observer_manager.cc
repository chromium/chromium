// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_event_observer_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/util/status.h"

namespace reporting {

MetricEventObserverManager::MetricEventObserverManager(
    std::unique_ptr<MetricEventObserver> event_observer,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    std::vector<Sampler*> additional_samplers)
    : event_observer_(std::move(event_observer)),
      metric_report_queue_(metric_report_queue),
      additional_samplers_collector_(
          std::make_unique<AdditionalSamplersCollector>(
              std::move(additional_samplers))) {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DETACH_FROM_SEQUENCE(sequence_checker_);

  auto on_event_observed_cb =
      base::BindRepeating(&MetricEventObserverManager::OnEventObserved,
                          weak_ptr_factory_.GetWeakPtr());
  event_observer_->SetOnEventObservedCallback(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(on_event_observed_cb)));

  reporting_controller_ = std::make_unique<MetricReportingController>(
      reporting_settings, enable_setting_path, setting_enabled_default_value,
      base::BindRepeating(&MetricEventObserverManager::SetReportingEnabled,
                          base::Unretained(this),
                          /*is_enabled=*/true),
      base::BindRepeating(&MetricEventObserverManager::SetReportingEnabled,
                          base::Unretained(this),
                          /*is_enabled=*/false));
}

MetricEventObserverManager::~MetricEventObserverManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MetricEventObserverManager::SetReportingEnabled(bool is_enabled) {
  is_reporting_enabled_ = is_enabled;
  event_observer_->SetReportingEnabled(is_enabled);
}

void MetricEventObserverManager::OnEventObserved(MetricData metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_reporting_enabled_) {
    return;
  }

  metric_data.set_timestamp_ms(base::Time::Now().ToJavaTime());

  additional_samplers_collector_->CollectAll(
      base::BindOnce(&MetricEventObserverManager::Report,
                     base::Unretained(this)),
      std::move(metric_data));
}

void MetricEventObserverManager::Report(
    absl::optional<MetricData> metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metric_data.has_value()) {
    NOTREACHED() << "Reporting requested for empty metric data.";
    return;
  }

  auto enqueue_cb = base::BindOnce([](Status status) {
    if (!status.ok()) {
      DVLOG(1)
          << "Could not enqueue observed event to reporting queue because of: "
          << status;
    }
  });
  metric_report_queue_->Enqueue(
      std::make_unique<MetricData>(std::move(metric_data.value())),
      std::move(enqueue_cb));
}
}  // namespace reporting
