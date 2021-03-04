// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORIES_CORE_VISIT_DATA_H_
#define COMPONENTS_MEMORIES_CORE_VISIT_DATA_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

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
  int64_t page_end_reason = 0;
};

struct MemoriesVisit {
  // The GURL of the visited page.
  GURL url;

  // The visit time.
  base::Time visit_time;

  VisitContextSignals context_signals;
};

}  // namespace memories

#endif  // COMPONENTS_MEMORIES_CORE_VISIT_DATA_H_
