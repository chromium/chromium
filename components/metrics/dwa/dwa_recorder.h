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

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"
#include "third_party/metrics_proto/dwa/deidentified_web_analytics.pb.h"

namespace metrics::dwa {

// Enables DWA recording.
COMPONENT_EXPORT(DWA) BASE_DECLARE_FEATURE(kDwaFeature);

class COMPONENT_EXPORT(DWA) DwaRecorder {
 public:
  DwaRecorder();

  DwaRecorder(const DwaRecorder&) = delete;
  DwaRecorder& operator=(const DwaRecorder&) = delete;

  ~DwaRecorder();

  void EnableRecording();
  void DisableRecording();

  // Deletes all unsent entries and page load events.
  void Purge();

  // Returns whether this DwaRecorder is enabled.
  bool IsEnabled();

  // Provides access to a global DwaRecorder instance for recording metrics.
  // This is typically passed to the Record() method of an entry object from
  // dwa_builders.h.
  static DwaRecorder* Get();

  // Saves all entries into a page load event on every page load. This method is
  // called once per page load. The purpose this needs to be called once per
  // page load is because the dwa proto collects aggregates events in terms of
  // "page load events".
  // TODO(b/369473036): Bind OnPageLoad method to call on every page load
  void OnPageLoad();

  // Adds an entry to the DwaEntry list.
  void AddEntry(metrics::dwa::mojom::DwaEntryPtr entry);

  // Returns true if DwaEntry list contains entries.
  bool HasEntries();

  // Takes all existing |page_load_events_| out from DwaRecorder and returns it.
  std::vector<::dwa::PageLoadEvents> TakePageLoadEvents();

  // Returns true if |page_load_events_| is non-empty.
  bool HasPageLoadEvents();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Local storage for the list of entries.
  std::vector<::metrics::dwa::mojom::DwaEntryPtr> entries_;

  // Local storage for the entries for page load events.
  std::vector<::dwa::PageLoadEvents> page_load_events_;

  bool recorder_enabled_ = false;
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_RECORDER_H_
