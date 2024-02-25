// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"

#include <memory>
#include <string>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting::test {

FakeMetricReportQueue::FakeMetricReportQueue(Priority priority)
    : MetricReportQueue(
          std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(
                  base::SequencedTaskRunner::GetCurrentDefault())),
          priority) {}

FakeMetricReportQueue::FakeMetricReportQueue(
    Priority priority,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : MetricReportQueue(
          std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(
                  base::SequencedTaskRunner::GetCurrentDefault())),
          priority,
          reporting_settings,
          rate_setting_path,
          default_rate,
          rate_unit_to_ms) {}

void FakeMetricReportQueue::Enqueue(MetricData metric_data,
                                    ReportQueue::EnqueueCallback callback) {
  reported_data_.AddValue(std::move(metric_data));
  std::move(callback).Run(Status());
}

FakeMetricReportQueue::~FakeMetricReportQueue() = default;

void FakeMetricReportQueue::Flush() {
  num_flush_++;
}

MetricData FakeMetricReportQueue::GetMetricDataReported() {
  return reported_data_.Take();
}

int FakeMetricReportQueue::GetNumFlush() const {
  return num_flush_;
}

bool FakeMetricReportQueue::IsEmpty() const {
  return reported_data_.IsEmpty();
}

Destination FakeMetricReportQueue::GetDestination() const {
  return Destination::UNDEFINED_DESTINATION;
}
}  // namespace reporting::test
