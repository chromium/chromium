// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKES_FAKE_METRIC_EVENT_OBSERVER_H_
#define COMPONENTS_REPORTING_METRICS_FAKES_FAKE_METRIC_EVENT_OBSERVER_H_

#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting::test {

class FakeMetricEventObserver : public MetricEventObserver {
 public:
  FakeMetricEventObserver();

  FakeMetricEventObserver(const FakeMetricEventObserver& other) = delete;
  FakeMetricEventObserver& operator=(const FakeMetricEventObserver& other) =
      delete;

  ~FakeMetricEventObserver() override;

  void SetOnEventObservedCallback(MetricRepeatingCallback cb) override;

  void SetReportingEnabled(bool is_enabled) override;

  void RunCallback(MetricData metric_data);

  bool GetReportingEnabled() const;

 private:
  bool is_reporting_enabled_ = false;

  MetricRepeatingCallback cb_;
};

}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_METRICS_FAKES_FAKE_METRIC_EVENT_OBSERVER_H_
