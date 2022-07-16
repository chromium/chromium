// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_data_collector.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/util/status.h"

namespace reporting {

CollectorBase::CollectorBase(Sampler* sampler,
                             MetricReportQueue* metric_report_queue)
    : sampler_(sampler), metric_report_queue_(metric_report_queue) {}

CollectorBase::~CollectorBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CollectorBase::Collect() {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto on_collected_cb = base::BindOnce(&CollectorBase::OnMetricDataCollected,
                                        weak_ptr_factory_.GetWeakPtr());
  sampler_->Collect(base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                       std::move(on_collected_cb)));
}

void CollectorBase::ReportMetricData(const MetricData& metric_data,
                                     base::OnceClosure on_data_reported) {
  auto enqueue_cb = base::BindOnce(
      [](base::OnceClosure on_data_reported, Status status) {
        if (!status.ok()) {
          DVLOG(1) << "Could not enqueue event to reporting queue because of: "
                   << status;
        }
        std::move(on_data_reported).Run();
      },
      std::move(on_data_reported));
  metric_report_queue_->Enqueue(metric_data, std::move(enqueue_cb));
}

OneShotCollector::OneShotCollector(Sampler* sampler,
                                   MetricReportQueue* metric_report_queue,
                                   ReportingSettings* reporting_settings,
                                   const std::string& setting_path,
                                   base::OnceClosure on_data_reported)
    : CollectorBase(sampler, metric_report_queue),
      on_data_reported_(std::move(on_data_reported)) {
  reporting_controller_ = std::make_unique<MetricReportingController>(
      reporting_settings, setting_path,
      base::BindRepeating(&OneShotCollector::Collect, base::Unretained(this)));
}

OneShotCollector::~OneShotCollector() = default;

void OneShotCollector::Collect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_collected_) {
    return;
  }
  data_collected_ = true;
  reporting_controller_.reset();
  CollectorBase::Collect();
}

void OneShotCollector::OnMetricDataCollected(MetricData metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(on_data_reported_);

  metric_data.set_timestamp_ms(base::Time::Now().ToJavaTime());
  ReportMetricData(metric_data, std::move(on_data_reported_));
}

PeriodicCollector::PeriodicCollector(Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     ReportingSettings* reporting_settings,
                                     const std::string& enable_setting_path,
                                     const std::string& rate_setting_path,
                                     base::TimeDelta default_rate,
                                     int rate_unit_to_ms)
    : CollectorBase(sampler, metric_report_queue),
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
          base::BindRepeating(&PeriodicEventCollector::StartPeriodicCollection,
                              base::Unretained(this)),
          base::BindRepeating(&PeriodicEventCollector::StopPeriodicCollection,
                              base::Unretained(this)))) {}

PeriodicCollector::~PeriodicCollector() = default;

void PeriodicCollector::OnMetricDataCollected(MetricData metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metric_data.set_timestamp_ms(base::Time::Now().ToJavaTime());
  ReportMetricData(metric_data);
}

void PeriodicCollector::StartPeriodicCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rate_controller_->Start();
}

void PeriodicCollector::StopPeriodicCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rate_controller_->Stop();
}

PeriodicEventCollector::PeriodicEventCollector(
    Sampler* sampler,
    std::unique_ptr<EventDetector> event_detector,
    std::vector<Sampler*> additional_samplers,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : PeriodicCollector(sampler,
                        metric_report_queue,
                        reporting_settings,
                        enable_setting_path,
                        rate_setting_path,
                        default_rate,
                        rate_unit_to_ms),
      additional_samplers_(std::move(additional_samplers)),
      event_detector_(std::move(event_detector)) {}

PeriodicEventCollector::~PeriodicEventCollector() = default;

void PeriodicEventCollector::OnMetricDataCollected(MetricData metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metric_data.set_timestamp_ms(base::Time::Now().ToJavaTime());
  const bool event_detected =
      event_detector_->DetectEvent(last_collected_data_, &metric_data);
  last_collected_data_ = std::move(metric_data);
  if (!event_detected) {
    return;
  }

  MetricData empty_metric_data;
  CollectAdditionalMetricData(/*sampler_index=*/0,
                              /*metric_data=*/last_collected_data_,
                              /*new_metric_data=*/empty_metric_data);
}

void PeriodicEventCollector::CollectAdditionalMetricData(
    uint64_t sampler_index,
    MetricData metric_data,
    MetricData new_metric_data) {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metric_data.CheckTypeAndMergeFrom(new_metric_data);
  if (sampler_index == additional_samplers_.size()) {
    ReportMetricData(metric_data);
    return;
  }

  auto on_collected_cb =
      base::BindOnce(&PeriodicEventCollector::CollectAdditionalMetricData,
                     weak_ptr_factory_.GetWeakPtr(), sampler_index + 1,
                     std::move(metric_data));
  additional_samplers_[sampler_index]->Collect(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(on_collected_cb)));
}
}  // namespace reporting
