// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTORY_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTORY_H_

#include <stddef.h>

#include <list>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/prerender_origin.h"
#include "url/gurl.h"

namespace prerender {

// PrerenderHistory maintains a per-session history of prerendered pages
// and their final dispositions. It has a fixed maximum capacity, and old
// items in history will be removed when the capacity is reached.
class PrerenderHistory {
 public:
  // Entry is an individual entry in the history list. It corresponds to a
  // specific prerendered page.
  struct Entry {
    Entry() : final_status(FINAL_STATUS_UNKNOWN), origin(ORIGIN_MAX) {}

    Entry(const GURL& url_arg,
          FinalStatus final_status_arg,
          Origin origin_arg,
          base::Time end_time_arg)
        : url(url_arg),
          final_status(final_status_arg),
          origin(origin_arg),
          end_time(end_time_arg) {}

    // The URL which was prerendered. This should be the URL included in the
    // <link rel="prerender"> tag, and not any URLs which it may have redirected
    // to.
    GURL url;

    // The FinalStatus describing whether the prerendered page was used or why
    // it was cancelled.
    FinalStatus final_status;

    // The Origin describing where the prerender originated from.
    Origin origin;

    // Time the NoStatePrefetchContents was destroyed.
    base::Time end_time;
  };

  // Creates a history with capacity for |max_items| entries.
  explicit PrerenderHistory(size_t max_items);

  PrerenderHistory(const PrerenderHistory&) = delete;
  PrerenderHistory& operator=(const PrerenderHistory&) = delete;

  ~PrerenderHistory();

  // Adds |entry| to the history. If at capacity, the oldest entry is dropped.
  void AddEntry(const Entry& entry);

  // Deletes all history entries.
  void Clear();

  // Retrieves the entries as a list of values which can be displayed.
  base::Value::List CopyEntriesAsValue() const;

 private:
  std::list<Entry> entries_;
  size_t max_items_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace prerender
#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTORY_H_
