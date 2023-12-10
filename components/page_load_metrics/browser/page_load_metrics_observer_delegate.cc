// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"

namespace page_load_metrics {

PageLoadMetricsObserverDelegate::BackForwardCacheRestore::
    BackForwardCacheRestore(bool was_in_foreground,
                            base::TimeTicks navigation_start_time)
    : navigation_start_time(navigation_start_time),
      was_in_foreground(was_in_foreground) {}

PageLoadMetricsObserverDelegate::BackForwardCacheRestore::
    BackForwardCacheRestore(const BackForwardCacheRestore&) = default;

bool PageLoadMetricsObserverDelegate::IsInPrerenderingBeforeActivationStart()
    const {
  switch (GetPrerenderingState()) {
    case PrerenderingState::kNoPrerendering:
    case PrerenderingState::kInPreview:
      return false;
    case PrerenderingState::kInPrerendering:
    case PrerenderingState::kActivatedNoActivationStart:
      return true;
    case PrerenderingState::kActivated:
      return false;
  }
}

}  // namespace page_load_metrics
