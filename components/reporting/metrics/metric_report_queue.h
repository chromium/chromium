// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_METRIC_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_METRICS_METRIC_REPORT_QUEUE_H_

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

class MetricData;
class MetricRateController;
class ReportingSettings;

// Simple wrapper for `::reporting::ReportQueue` that can be set for periodic
// upload.
class MetricReportQueue {
 public:
  MetricReportQueue(
      std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue,
      Priority priority);

  // Constructor used if the reporter is needed to upload records periodically.
  MetricReportQueue(
      std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue,
      Priority priority,
      ReportingSettings* reporting_settings,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms = 1);

  MetricReportQueue() = delete;
  MetricReportQueue(const MetricReportQueue& other) = delete;
  MetricReportQueue& operator=(const MetricReportQueue& other) = delete;

  virtual ~MetricReportQueue();

  // Enqueue the metric data.
  virtual void Enqueue(
      MetricData metric_data,
      ReportQueue::EnqueueCallback callback = base::DoNothing());

  // Initiate manual upload of records with `priority_` and restart timer if
  // exists.
  void Upload();

  // Retrieves the reporting destination configured with the `report_queue_`.
  virtual Destination GetDestination() const;

 private:
  // Initiate upload of records with `priority_`.
  virtual void Flush();

  const std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue_;

  const Priority priority_;

  std::unique_ptr<MetricRateController> rate_controller_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_METRIC_REPORT_QUEUE_H_
