// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_event_observer_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

MetricEventObserverManager::MetricEventObserverManager(
    std::unique_ptr<MetricEventObserver> event_observer,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    EventDrivenTelemetryCollectorPool* collector_pool,
    base::TimeDelta init_delay)
    : event_observer_(std::move(event_observer)),
      metric_report_queue_(metric_report_queue),
      collector_pool_(collector_pool) {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  DETACH_FROM_SEQUENCE(sequence_checker_);

  auto on_event_observed_cb =
      base::BindRepeating(&MetricEventObserverManager::OnEventObserved,
                          weak_ptr_factory_.GetWeakPtr());
  event_observer_->SetOnEventObservedCallback(
      base::BindPostTaskToCurrentDefault(std::move(on_event_observed_cb)));

  reporting_controller_ = std::make_unique<MetricReportingController>(
      reporting_settings, enable_setting_path, setting_enabled_default_value);

  CHECK(!init_delay.is_negative());
  if (init_delay.is_zero()) {
    SetReportingControllerCb();
    return;
  }
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricEventObserverManager::SetReportingControllerCb,
                     weak_ptr_factory_.GetWeakPtr()),
      init_delay);
}

MetricEventObserverManager::~MetricEventObserverManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MetricEventObserverManager::SetReportingControllerCb() {
  reporting_controller_->SetSettingUpdateCb(
      base::BindRepeating(&MetricEventObserverManager::SetReportingEnabled,
                          base::Unretained(this),
                          /*is_enabled=*/true),
      base::BindRepeating(&MetricEventObserverManager::SetReportingEnabled,
                          base::Unretained(this),
                          /*is_enabled=*/false));
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
  MetricEventType event_type = metric_data.event_data().type();
  metric_data.set_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  metric_report_queue_->Enqueue(std::move(metric_data));
  base::UmaHistogramEnumeration(kEventMetricEnqueuedMetricsName, event_type,
                                MetricEventType_MAX);

  if (collector_pool_) {
    std::vector<raw_ptr<CollectorBase, VectorExperimental>>
        telemetry_collectors =
            collector_pool_->GetTelemetryCollectors(event_type);
    for (CollectorBase* telemetry_collector : telemetry_collectors) {
      telemetry_collector->Collect(/*is_event_driven=*/true);
    }
  }
}
}  // namespace reporting
