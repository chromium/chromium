// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_data_collector.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/metric_reporting_controller.h"
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
  sampler_->MaybeCollect(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(on_collected_cb)));
}

void CollectorBase::ReportMetricData(MetricData metric_data,
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
  metric_report_queue_->Enqueue(
      std::make_unique<MetricData>(std::move(metric_data)),
      std::move(enqueue_cb));
}

OneShotCollector::OneShotCollector(Sampler* sampler,
                                   MetricReportQueue* metric_report_queue,
                                   ReportingSettings* reporting_settings,
                                   const std::string& setting_path,
                                   bool setting_enabled_default_value,
                                   base::OnceClosure on_data_reported)
    : CollectorBase(sampler, metric_report_queue),
      on_data_reported_(std::move(on_data_reported)) {
  reporting_controller_ = std::make_unique<MetricReportingController>(
      reporting_settings, setting_path, setting_enabled_default_value,
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

void OneShotCollector::OnMetricDataCollected(
    absl::optional<MetricData> metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(on_data_reported_);
  if (!metric_data.has_value()) {
    return;
  }

  metric_data->set_timestamp_ms(base::Time::Now().ToJavaTime());
  ReportMetricData(std::move(metric_data.value()),
                   std::move(on_data_reported_));
}

PeriodicCollector::PeriodicCollector(Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     ReportingSettings* reporting_settings,
                                     const std::string& enable_setting_path,
                                     bool setting_enabled_default_value,
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
          setting_enabled_default_value,
          base::BindRepeating(&PeriodicEventCollector::StartPeriodicCollection,
                              base::Unretained(this)),
          base::BindRepeating(&PeriodicEventCollector::StopPeriodicCollection,
                              base::Unretained(this)))) {}

PeriodicCollector::~PeriodicCollector() = default;

void PeriodicCollector::OnMetricDataCollected(
    absl::optional<MetricData> metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metric_data.has_value()) {
    return;
  }

  metric_data->set_timestamp_ms(base::Time::Now().ToJavaTime());
  ReportMetricData(std::move(metric_data.value()));
}

void PeriodicCollector::StartPeriodicCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do initial collection at startup.
  Collect();
  rate_controller_->Start();
}

void PeriodicCollector::StopPeriodicCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rate_controller_->Stop();
}

AdditionalSamplersCollector::AdditionalSamplersCollector(
    std::vector<Sampler*> samplers)
    : samplers_(std::move(samplers)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AdditionalSamplersCollector::~AdditionalSamplersCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AdditionalSamplersCollector::CollectAll(
    OptionalMetricCallback on_all_collected_cb,
    MetricData metric_data) const {
  CollectAdditionalMetricData(
      /*sampler_index=*/0, std::move(on_all_collected_cb),
      std::move(metric_data), /*new_metric_data=*/absl::nullopt);
}

void AdditionalSamplersCollector::CollectAdditionalMetricData(
    uint64_t sampler_index,
    OptionalMetricCallback on_all_collected_cb,
    MetricData metric_data,
    absl::optional<MetricData> new_metric_data) const {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_metric_data.has_value()) {
    metric_data.CheckTypeAndMergeFrom(new_metric_data.value());
  }
  if (sampler_index == samplers_.size()) {
    std::move(on_all_collected_cb).Run(std::move(metric_data));
    return;
  }

  auto on_collected_cb =
      base::BindOnce(&AdditionalSamplersCollector::CollectAdditionalMetricData,
                     weak_ptr_factory_.GetWeakPtr(), sampler_index + 1,
                     std::move(on_all_collected_cb), std::move(metric_data));
  samplers_[sampler_index]->MaybeCollect(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(on_collected_cb)));
}

PeriodicEventCollector::PeriodicEventCollector(
    Sampler* sampler,
    std::unique_ptr<EventDetector> event_detector,
    std::vector<Sampler*> additional_samplers,
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
                        rate_unit_to_ms),
      event_detector_(std::move(event_detector)),
      additional_samplers_collector_(
          std::make_unique<AdditionalSamplersCollector>(
              std::move(additional_samplers))) {}

PeriodicEventCollector::~PeriodicEventCollector() = default;

void PeriodicEventCollector::OnMetricDataCollected(
    absl::optional<MetricData> metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metric_data.has_value()) {
    return;
  }

  metric_data->set_timestamp_ms(base::Time::Now().ToJavaTime());
  absl::optional<MetricEventType> event =
      event_detector_->DetectEvent(last_collected_data_, metric_data.value());
  last_collected_data_ = std::move(metric_data.value());
  if (!event.has_value()) {
    return;
  }
  last_collected_data_.mutable_event_data()->set_type(event.value());

  additional_samplers_collector_->CollectAll(
      base::BindOnce(&PeriodicEventCollector::OnAdditionalMetricDataCollected,
                     base::Unretained(this)),
      /*metric_data=*/last_collected_data_);
}

void PeriodicEventCollector::OnAdditionalMetricDataCollected(
    absl::optional<MetricData> metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metric_data.has_value()) {
    NOTREACHED() << "Metric data is unexpectedly empty after additional metric "
                 << "collection.";
    return;
  }

  ReportMetricData(std::move(metric_data.value()));
}
}  // namespace reporting
