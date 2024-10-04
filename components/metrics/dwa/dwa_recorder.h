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
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"
#include "third_party/metrics_proto/dwa/deidentified_web_analytics.pb.h"

namespace metrics::dwa {

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

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // TODO(b/369464150): Pending completion of the listed bug, we should put this
  // function as free functions in the anonymous namespace of cc file.
  // Takes a vector of entries, aggregates them, and then returns a vector of
  // dwa events.
  static std::vector<::dwa::DeidentifiedWebAnalyticsEvent> BuildDwaEvents(
      const std::vector<::metrics::dwa::mojom::DwaEntryPtr>& entries);

  // TODO(b/369464150): Pending completion of the listed bug, we should put this
  // function as free functions in the anonymous namespace of cc file.
  // Populates |dwa_entry|.field_trials with the field trial/group name hashes
  // for the field_trials we are interested in, |studies_of_interest|.
  // |active_field_trial_groups| contains a mapping of field trial names to
  // group names that we are currently part of.
  static void PopulateFieldTrialsForDwaEvent(
      const base::flat_map<std::string, bool>& studies_of_interest,
      const std::unordered_map<std::string, std::string>&
          active_field_trial_groups,
      ::dwa::DeidentifiedWebAnalyticsEvent& dwa_event);

  // Local storage for the list of entries.
  std::vector<::metrics::dwa::mojom::DwaEntryPtr> entries_;

  // Local storage for the entries for page load events.
  std::vector<::dwa::PageLoadEvents> page_load_events_;

  bool recorder_enabled_ = false;
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_RECORDER_H_
