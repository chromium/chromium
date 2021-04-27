// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_VISIT_DATA_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_VISIT_DATA_H_

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// Tracks which fields have been or are pending recording. This helps 1) avoid
// re-recording fields and 2) determine whether a visit is compete (i.e. has all
// expected fields recorded).
struct RecordingStatus {
  // Whether |url_row| and |visit_row| have been set.
  bool history_rows = false;
  // Whether a navigation has ended; i.e. another navigation has began in the
  // same tab or the navigation's tab has been closed.
  bool navigation_ended = false;
  // Whether the |context_signals| associated with navigation end have been set.
  // Should only be true if both |history_rows| and |navigation_ended| are true.
  bool navigation_end_signals = false;
  // Whether the UKM |page_end_reason| |context_signal| is expected to be set.
  bool expect_ukm_page_end_signals = false;
  // Whether the UKM |page_end_reason| |context_signal| has been set. Should
  // only be true if |expect_ukm_page_end_signals| is true.
  bool ukm_page_end_signals = false;
};

// A partially built visit with its state of completeness.
struct IncompleteVisit : history::ClusterVisit {
  RecordingStatus status;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_VISIT_DATA_H_
