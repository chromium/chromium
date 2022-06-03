// Copyright 2020 The Chromium Authors. All rights reserved.
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

}  // namespace page_load_metrics
