// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Usage example:
//
// At metrics collection site:
// dwa::builders::MyEvent(source_id)
//    .SetMyMetric(metric_value)
//    .Record(dwa_recorder.get());
//------------------------------------------------------------------------------

#ifndef COMPONENTS_METRICS_DWA_DWA_RECORDER_H_
#define COMPONENTS_METRICS_DWA_DWA_RECORDER_H_

#include "base/component_export.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"

namespace metrics::dwa {

class COMPONENT_EXPORT(DWA) DwaRecorder {
 public:
  DwaRecorder();

  DwaRecorder(const DwaRecorder&) = delete;
  DwaRecorder& operator=(const DwaRecorder&) = delete;

  ~DwaRecorder();

  // Provides access to a global DwaRecorder instance for recording metrics.
  // This is typically passed to the Record() method of an entry object from
  // dwa_builders.h.
  static DwaRecorder* Get();

  // Adds an entry to the DwaEntry list.
  static void AddEntry(metrics::dwa::mojom::DwaEntryPtr entry);
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_RECORDER_H_
