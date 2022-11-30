// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_event_observer_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/event_driven_telemetry_sampler_pool.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/multi_samplers_collector.h"
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
    EventDrivenTelemetrySamplerPool* sampler_pool)
    : event_observer_(std::move(event_observer)),
      metric_report_queue_(metric_report_queue),
      sampler_pool_(sampler_pool) {
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

  std::vector<ConfiguredSampler*> telemetry_samplers;
  if (sampler_pool_) {
    telemetry_samplers =
        sampler_pool_->GetTelemetrySamplers(metric_data.event_data().type());
  }
  auto collect_cb =
      base::BindOnce(&MetricEventObserverManager::MergeAndReport,
                     weak_ptr_factory_.GetWeakPtr(), std::move(metric_data));
  MultiSamplersCollector::CollectAll(telemetry_samplers, std::move(collect_cb));
}

void MetricEventObserverManager::MergeAndReport(
    MetricData event_data,
    absl::optional<MetricData> telemetry_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MetricData metric_data = std::move(event_data);
  if (telemetry_data.has_value()) {
    metric_data.CheckTypeAndMergeFrom(telemetry_data.value());
  }

  auto enqueue_cb = base::BindOnce([](Status status) {
    if (!status.ok()) {
      DVLOG(1)
          << "Could not enqueue observed event to reporting queue because of: "
          << status;
    }
  });
  metric_report_queue_->Enqueue(
      std::make_unique<MetricData>(std::move(metric_data)),
      std::move(enqueue_cb));
}
}  // namespace reporting
