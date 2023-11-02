// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULING_METRICS_TASK_DURATION_METRIC_REPORTER_H_
#define COMPONENTS_SCHEDULING_METRICS_TASK_DURATION_METRIC_REPORTER_H_

#include <memory>

#include "base/component_export.h"
#include "base/metrics/histogram.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"

namespace base {
class HistogramBase;
}

namespace scheduling_metrics {

// A helper class to report total task runtime split by the different types of
// |TypeClass|. Only full seconds are reported. Note that partial seconds are
// rounded up/down, so that on average the correct value is reported when many
// reports are added.
//
// |TaskClass| is an enum which should have kClass field.
template <class TaskClass>
class TaskDurationMetricReporter {
 public:
  // Note that 1000*1000 is used to get microseconds precision.
  explicit TaskDurationMetricReporter(const char* metric_name)
      : value_per_type_histogram_(new base::ScaledLinearHistogram(
            metric_name,
            1,
            static_cast<int>(TaskClass::kMaxValue) + 1,
            static_cast<int>(TaskClass::kMaxValue) + 2,
            1000 * 1000,
            base::HistogramBase::kUmaTargetedHistogramFlag)) {}

  TaskDurationMetricReporter(const TaskDurationMetricReporter&) = delete;
  TaskDurationMetricReporter& operator=(const TaskDurationMetricReporter&) =
      delete;

  void RecordTask(TaskClass task_class, base::TimeDelta duration) {
    DCHECK_LT(static_cast<int>(task_class),
              static_cast<int>(TaskClass::kMaxValue) + 1);

    // To get microseconds precision, duration is converted to microseconds
    // since |value_per_type_histogram_| is constructed with a scale of
    // 1000*1000.
    const int task_micros =
        base::saturated_cast<int>(duration.InMicroseconds());
    if (task_micros > 0) {
      value_per_type_histogram_->AddScaledCount(static_cast<int>(task_class),
                                                task_micros);
    }
  }

 private:
  std::unique_ptr<base::ScaledLinearHistogram> value_per_type_histogram_;
};

}  // namespace scheduling_metrics

#endif  // COMPONENTS_SCHEDULING_METRICS_TASK_DURATION_METRIC_REPORTER_H_
