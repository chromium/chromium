// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_VISIT_DATA_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_VISIT_DATA_H_

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"

namespace memories {

// Context signals about a page visit collected during the page lifetime.
// This struct encapsulates data that's shared between UKM and the on-device
// storage for Memories metadata, recorded to both when the page lifetime ends.
// This is to ensure that History actually has the visit row already written.
struct VisitContextSignals {
  // True if the user has cut or copied the omnibox URL to the clipboard for
  // this page load.
  bool omnibox_url_copied = false;

  // True if the page was in a tab group when the navigation was committed.
  bool is_existing_part_of_tab_group = false;

  // True if the page was NOT part of a tab group when the navigation
  // committed, and IS part of a tab group at the end of the page lifetime.
  bool is_placed_in_tab_group = false;

  // True if this page was a bookmark when the navigation was committed.
  bool is_existing_bookmark = false;

  // True if the page was NOT a bookmark when the navigation was committed and
  // was MADE a bookmark during the page's lifetime. In other words:
  // If |is_existing_bookmark| is true, that implies |is_new_bookmark| is false.
  bool is_new_bookmark = false;

  // True if the page has been explicitly added (by the user) to the list of
  // custom links displayed in the NTP. Links added to the NTP by History
  // TopSites don't count for this.  Always false on Android, because Android
  // does not have NTP custom links.
  bool is_ntp_custom_link = false;

  // The duration since the last visit to this URL in seconds, if the user has
  // visited the URL before. Recorded as -1 if the user has not visited the URL
  // before, or if the History service is unavailable or slow to respond. Any
  // duration that exceeds 30 days will be recorded as 30 days, so in practice,
  // if this duration indicates 30 days, it can be anything from 30 to the
  // maximum duration that local history is stored.
  int64_t duration_since_last_visit_seconds = -1;

  // ---------------------------------------------------------------------------
  // The below metrics are all already recorded by UKM for non-memories reasons.
  // We are duplicating them below to persist on-device and send to an offline
  // model.

  // An opaque integer representing page_load_metrics::PageEndReason.
  // Do not use this directly, as it's a raw integer for serialization, and not
  // a typesafe page_load_metrics::PageEndReason.
  int page_end_reason = 0;
};

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

struct MemoriesVisit {
  history::URLRow url_row;
  history::VisitRow visit_row;
  VisitContextSignals context_signals;
  RecordingStatus status;
};

}  // namespace memories

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_VISIT_DATA_H_
