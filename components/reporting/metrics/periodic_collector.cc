// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/periodic_collector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/sampler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

PeriodicCollector::PeriodicCollector(Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     ReportingSettings* reporting_settings,
                                     const std::string& enable_setting_path,
                                     bool setting_enabled_default_value,
                                     const std::string& rate_setting_path,
                                     base::TimeDelta default_rate,
                                     int rate_unit_to_ms)
    : CollectorBase(sampler),
      metric_report_queue_(metric_report_queue),
      rate_controller_(std::make_unique<MetricRateController>(
          base::BindRepeating(&PeriodicCollector::Collect,
                              base::Unretained(this)),
          reporting_settings,
          rate_setting_path,
          default_rate,
          rate_unit_to_ms)),
      reporting_controller_(std::make_unique<MetricReportingController>(
          reporting_settings,
          enable_setting_path,
          setting_enabled_default_value)) {
  reporting_controller_->SetSettingUpdateCb(
      base::BindRepeating(&PeriodicCollector::StartPeriodicCollection,
                          base::Unretained(this)),
      base::BindRepeating(&PeriodicCollector::StopPeriodicCollection,
                          base::Unretained(this)));
}

PeriodicCollector::~PeriodicCollector() = default;

void PeriodicCollector::OnMetricDataCollected(
    absl::optional<MetricData> metric_data) {
  CheckOnSequence();
  if (!metric_data.has_value()) {
    return;
  }

  metric_data->set_timestamp_ms(base::Time::Now().ToJavaTime());
  metric_report_queue_->Enqueue(std::move(metric_data.value()));
}

void PeriodicCollector::StartPeriodicCollection() {
  CheckOnSequence();
  // Do initial collection at startup.
  Collect();
  rate_controller_->Start();
}

void PeriodicCollector::StopPeriodicCollection() {
  CheckOnSequence();
  rate_controller_->Stop();
}
}  // namespace reporting
