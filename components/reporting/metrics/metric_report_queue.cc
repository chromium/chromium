// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_report_queue.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

MetricReportQueue::MetricReportQueue(
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue,
    Priority priority)
    : report_queue_(std::move(report_queue)), priority_(priority) {}

MetricReportQueue::MetricReportQueue(
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue,
    Priority priority,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : MetricReportQueue(std::move(report_queue), priority) {
  rate_controller_ = std::make_unique<MetricRateController>(
      base::BindRepeating(&MetricReportQueue::Flush, base::Unretained(this)),
      reporting_settings, rate_setting_path, default_rate, rate_unit_to_ms);
  rate_controller_->Start();
}

MetricReportQueue::~MetricReportQueue() = default;

void MetricReportQueue::Enqueue(MetricData metric_data,
                                ReportQueue::EnqueueCallback callback) {
  auto enqueue_cb = base::BindOnce(
      [](ReportQueue::EnqueueCallback callback, Status status) {
        if (!status.ok()) {
          DVLOG(1) << "Could not enqueue to reporting queue because of: "
                   << status;
        }
        std::move(callback).Run(std::move(status));
      },
      std::move(callback));
  report_queue_->Enqueue(std::make_unique<MetricData>(std::move(metric_data)),
                         priority_, std::move(enqueue_cb));
}

void MetricReportQueue::Upload() {
  Flush();
  // Restart timer if the metric report queue flush is rate controlled.
  if (rate_controller_) {
    rate_controller_->Stop();
    rate_controller_->Start();
  }
}

Destination MetricReportQueue::GetDestination() const {
  CHECK(report_queue_);
  return report_queue_->GetDestination();
}

void MetricReportQueue::Flush() {
  report_queue_->Flush(
      priority_, base::BindOnce([](Status status) {
        if (!status.ok()) {
          DVLOG(1) << "Could not upload metric data records because of: "
                   << status;
        }
      }));
}
}  // namespace reporting
