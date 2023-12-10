// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKES_FAKE_METRIC_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_METRICS_FAKES_FAKE_METRIC_REPORT_QUEUE_H_

#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/repeating_test_future.h"
#include "base/time/time.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting::test {

class FakeMetricReportQueue : public MetricReportQueue {
 public:
  explicit FakeMetricReportQueue(Priority priority = Priority::IMMEDIATE);

  FakeMetricReportQueue(Priority priority,
                        ReportingSettings* reporting_settings,
                        const std::string& rate_setting_path,
                        base::TimeDelta default_rate,
                        int rate_unit_to_ms = 1);

  FakeMetricReportQueue(const FakeMetricReportQueue& other) = delete;
  FakeMetricReportQueue& operator=(const FakeMetricReportQueue& other) = delete;

  ~FakeMetricReportQueue() override;

  void Enqueue(
      MetricData metric_data,
      ReportQueue::EnqueueCallback callback = base::DoNothing()) override;

  MetricData GetMetricDataReported();

  int GetNumFlush() const;

  bool IsEmpty() const;

  Destination GetDestination() const override;

 private:
  void Flush() override;

  base::test::RepeatingTestFuture<MetricData> reported_data_;

  int num_flush_ = 0;
};
}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_METRICS_FAKES_FAKE_METRIC_REPORT_QUEUE_H_
