// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULING_METRICS_TOTAL_DURATION_METRIC_REPORTER_H_
#define COMPONENTS_SCHEDULING_METRICS_TOTAL_DURATION_METRIC_REPORTER_H_

#include "base/component_export.h"
#include "base/metrics/histogram.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace scheduling_metrics {

// Helper class to measure the total duration of the event accounting for
// possibility of the renderer process going away at any point.
//
// This is implemented by "undoing" old total value and "applying" new one
// each time RecordAdditionalDuration() is called.
// "Undoing" is implemented by having a second "negative" histogram, so to
// obtain the result, |positive_histogram - negative_histogram| difference
// should be analysed.
class COMPONENT_EXPORT(SCHEDULING_METRICS) TotalDurationMetricReporter {
 public:
  TotalDurationMetricReporter(const char* positive_histogram_name,
                              const char* negative_histogram_name);

  void RecordAdditionalDuration(base::TimeDelta duration);

  void Reset();

 private:
  base::Optional<base::TimeDelta> reported_value_;

  base::HistogramBase* positive_histogram_;
  base::HistogramBase* negative_histogram_;
};

}  // namespace scheduling_metrics

#endif  // COMPONENTS_SCHEDULING_METRICS_TOTAL_DURATION_METRIC_REPORTER_H_
