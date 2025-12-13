// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/prerender_prewarm_navigation_data.h"

namespace page_load_metrics {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PrerenderPrewarmNavigationData);

PrerenderPrewarmNavigationData::PrerenderPrewarmNavigationData(
    content::NavigationHandle& navigation_handle,
    bool prewarm_committed,
    bool prerender_host_reused)
    : prewarm_committed_(prewarm_committed),
      prerender_host_reused_(prerender_host_reused) {}

PrerenderPrewarmNavigationData::PrerenderPrewarmNavigationStatus
PrerenderPrewarmNavigationData::GetNavigationStatus() const {
  if (prewarm_committed_) {
    return prerender_host_reused_
               ? PrerenderPrewarmNavigationStatus::kPrewarmedReused
               : PrerenderPrewarmNavigationStatus::kPrewarmedNotReused;
  }
  return prerender_host_reused_
             ? PrerenderPrewarmNavigationStatus::kNotPrewarmedReused
             : PrerenderPrewarmNavigationStatus::kNotPrewarmedNotReused;
}

}  // namespace page_load_metrics
