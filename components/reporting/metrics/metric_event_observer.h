// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_METRIC_EVENT_OBSERVER_H_
#define COMPONENTS_REPORTING_METRICS_METRIC_EVENT_OBSERVER_H_

#include "base/functional/callback.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

using MetricRepeatingCallback = base::RepeatingCallback<void(MetricData)>;

// A `MetricEventObserver` object should observe events and report them using
// the `MetricRepeatingCallback` set using `SetOnEventObservedCallback`.
// Whether the object should observe/report events is controlled by is_enabled
// set using `SetReportingEnabled`.
class MetricEventObserver {
 public:
  virtual ~MetricEventObserver() = default;
  virtual void SetOnEventObservedCallback(MetricRepeatingCallback cb) = 0;
  virtual void SetReportingEnabled(bool is_enabled) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_METRIC_EVENT_OBSERVER_H_
