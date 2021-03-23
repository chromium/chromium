// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EVENT_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EVENT_H_

namespace page_load_metrics {

// Discrete events that can be broadcast to observers via
// MetricsWebContentsObserver::BroadcastEventToObservers. These values are only
// used internally and not persisted. They may be freely changed.
enum class PageLoadMetricsEvent {
  PREVIEWS_OPT_OUT,
  PREFETCH_LIKELY,
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EVENT_H_
