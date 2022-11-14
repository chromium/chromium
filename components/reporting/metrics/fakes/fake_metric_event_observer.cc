// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fakes/fake_metric_event_observer.h"

#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::test {

FakeMetricEventObserver::FakeMetricEventObserver() = default;

FakeMetricEventObserver::~FakeMetricEventObserver() = default;

void FakeMetricEventObserver::SetOnEventObservedCallback(
    MetricRepeatingCallback cb) {
  EXPECT_FALSE(cb_);
  cb_ = std::move(cb);
}

void FakeMetricEventObserver::SetReportingEnabled(bool is_enabled) {
  is_reporting_enabled_ = is_enabled;
}

void FakeMetricEventObserver::RunCallback(MetricData metric_data) {
  base::RunLoop run_loop;
  cb_.Run(std::move(metric_data));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

bool FakeMetricEventObserver::GetReportingEnabled() const {
  return is_reporting_enabled_;
}

}  // namespace reporting::test
