// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTOGRAMS_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTOGRAMS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/prerender_origin.h"
#include "url/gurl.h"

namespace prerender {

// Navigation type for histograms.
enum NavigationType {
  // A normal completed navigation.
  NAVIGATION_TYPE_NORMAL,
  // A completed navigation or swap that began as a prerender.
  NAVIGATION_TYPE_PRERENDERED,
};

// Records histograms for NoStatePrefetchManager.
//
// A few histograms are dynamically constructed to avoid binary size bloat from
// histogram_macros.h. Such histograms require careful handling:
// 1. slow - make sure only rare events are recorded this way, a handful of such
//    events per page load should be OK
// 2. may lead to small sporadic memory leaks in Histogram::Factory::Build() -
//    ensuring that they are recorded from the same thread is sufficient
//
// Besides thread checking this class is stateless, all public methods are
// const.
class PrerenderHistograms {
 public:
  // Owned by a NoStatePrefetchManager object for the lifetime of the
  // NoStatePrefetchManager.
  PrerenderHistograms();

  PrerenderHistograms(const PrerenderHistograms&) = delete;
  PrerenderHistograms& operator=(const PrerenderHistograms&) = delete;

  // Return the string to use as a prefix for histograms depending on the origin
  // of the prerender.
  static std::string GetHistogramPrefix(Origin origin);

  // Record a PerSessionCount data point.
  void RecordPerSessionCount(Origin origin, int count) const;

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin, FinalStatus final_status) const;

  // To be called when a new prerender is started.
  void RecordPrerenderStarted(Origin origin) const;

  // Record the histogram for number of bytes consumed by the prerender, and the
  // total number of bytes fetched for this profile since the last call to
  // RecordBytes.
  void RecordNetworkBytesConsumed(Origin origin,
                                  int64_t prerender_bytes,
                                  int64_t profile_bytes) const;

  // Records the time to first contentful paint with respect to a possible
  // prefetch of the page. The time to first contentful paint with respect to
  // the navigation start is recorded (even if the page was prererendered in
  // advance of navigation start). One of several histograms is used, depending
  // on whether this URL could have been prefetched before the navigation
  // leading to the paint.
  void RecordPrefetchFirstContentfulPaintTime(
      Origin origin,
      bool is_no_store,
      bool was_hidden,
      base::TimeDelta time,
      base::TimeDelta prefetch_age) const;

 private:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTOGRAMS_H_
