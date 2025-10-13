// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_DKM_RECORDER_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_DKM_RECORDER_H_

#include <vector>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "components/metrics/private_metrics/mojom/private_metrics_interface.mojom.h"

namespace metrics::private_metrics {

// The interface for recording DKM (Domain Keyed Metrics).
//
// A example usage at the metrics collection site is:
// dkm::builders::MyEvent(source_id)
//    .SetMyMetric(metric_value)
//    .Record(DkmRecorder::Get());
class COMPONENT_EXPORT(PRIVATE_METRICS_RECORDERS) DkmRecorder {
 public:
  DkmRecorder();

  DkmRecorder(const DkmRecorder&) = delete;
  DkmRecorder& operator=(const DkmRecorder&) = delete;

  ~DkmRecorder();

  // Provides access to a global DkmRecorder instance for recording metrics.
  // This is typically passed to the Record() method of an entry object from
  // dkm_builders.h.
  static DkmRecorder* Get();

  // Adds an entry to the PrivateMetricsEntry list.
  void AddEntry(mojom::PrivateMetricsEntryPtr entry);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Local storage for the list of entries.
  std::vector<mojom::PrivateMetricsEntryPtr> entries_;
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_DKM_RECORDER_H_
