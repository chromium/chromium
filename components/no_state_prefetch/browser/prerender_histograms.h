// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTOGRAMS_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTOGRAMS_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"

namespace prerender {

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
  PrerenderHistograms() = default;

  PrerenderHistograms(const PrerenderHistograms&) = delete;
  PrerenderHistograms& operator=(const PrerenderHistograms&) = delete;

  // Return the string to use as a prefix for histograms depending on the origin
  // of the prerender.
  static std::string GetHistogramPrefix(Origin origin);

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin, FinalStatus final_status) const;

 private:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_HISTOGRAMS_H_
