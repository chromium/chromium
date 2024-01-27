// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/periodic_collector.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

PeriodicCollector::PeriodicCollector(Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     ReportingSettings* reporting_settings,
                                     const std::string& enable_setting_path,
                                     bool setting_enabled_default_value,
                                     const std::string& rate_setting_path,
                                     base::TimeDelta default_rate,
                                     int rate_unit_to_ms,
                                     base::TimeDelta init_delay)
    : CollectorBase(sampler),
      metric_report_queue_(metric_report_queue),
      rate_controller_(std::make_unique<MetricRateController>(
          base::BindRepeating(&PeriodicCollector::Collect,
                              base::Unretained(this),
                              /*is_event_driven=*/false),
          reporting_settings,
          rate_setting_path,
          default_rate,
          rate_unit_to_ms)),
      reporting_controller_(std::make_unique<MetricReportingController>(
          reporting_settings,
          enable_setting_path,
          setting_enabled_default_value)) {
  if (init_delay.is_zero()) {
    SetReportingControllerCb();
    return;
  }
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PeriodicCollector::SetReportingControllerCb,
                     weak_ptr_factory_.GetWeakPtr()),
      init_delay);
}

PeriodicCollector::PeriodicCollector(Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     ReportingSettings* reporting_settings,
                                     const std::string& enable_setting_path,
                                     bool setting_enabled_default_value,
                                     const std::string& rate_setting_path,
                                     base::TimeDelta default_rate,
                                     int rate_unit_to_ms)
    : PeriodicCollector(sampler,
                        metric_report_queue,
                        reporting_settings,
                        enable_setting_path,
                        setting_enabled_default_value,
                        rate_setting_path,
                        default_rate,
                        rate_unit_to_ms,
                        base::TimeDelta()) {}

PeriodicCollector::~PeriodicCollector() = default;

void PeriodicCollector::OnMetricDataCollected(
    bool is_event_driven,
    std::optional<MetricData> metric_data) {
  CheckOnSequence();
  CHECK(metric_report_queue_);
  if (!metric_data.has_value()) {
    base::UmaHistogramExactLinear(
        PeriodicCollector::kNoMetricDataMetricsName,
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

bool PeriodicCollector::CanCollect() const {
  return reporting_controller_->IsEnabled();
}

void PeriodicCollector::StartPeriodicCollection() {
  CheckOnSequence();
  // Do initial collection at startup.
  Collect(/*is_event_driven=*/false);
  rate_controller_->Start();
}

void PeriodicCollector::StopPeriodicCollection() {
  CheckOnSequence();
  rate_controller_->Stop();
}

void PeriodicCollector::SetReportingControllerCb() {
  reporting_controller_->SetSettingUpdateCb(
      base::BindRepeating(&PeriodicCollector::StartPeriodicCollection,
                          base::Unretained(this)),
      base::BindRepeating(&PeriodicCollector::StopPeriodicCollection,
                          base::Unretained(this)));
}
}  // namespace reporting
