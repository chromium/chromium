// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_DATA_H_

#include "components/page_load_metrics/browser/interaction_to_next_paint_calculator.h"
#include "components/page_load_metrics/browser/layout_shift_normalization.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace page_load_metrics {

// Holds the metrics and metric aggregators for a single soft navigation.
//
// Note: `metrics` may be null if performance entries (e.g. event timings,
// layout shifts, or LCP candidates) arrived tagged with a `navigation_id`
// before the corresponding soft navigation commit arrived, and then the
// navigation bucket was finalized/flushed before receiving a valid commit IPC.
struct SoftNavigationData {
  SoftNavigationData();
  ~SoftNavigationData();

  // Records the timestamp of the first backgrounding event during this soft
  // navigation. Only the initial background time is recorded; subsequent calls
  // are ignored.
  void RecordFirstBackgroundTime(base::TimeDelta background_time);

  mojom::SoftNavigationMetricsPtr metrics;
  InteractionToNextPaintCalculator inp_calculator;
  LayoutShiftNormalization cls_calculator;
  LargestContentfulPaintHandler lcp_handler;
  // The timestamp when the page was backgrounded. If the soft navigation
  // started while the page was in the background, this holds the timestamp of
  // when that background period started (which will be <= soft navigation
  // start_time). If the soft navigation started in the foreground and was later
  // backgrounded, this holds the timestamp of the first backgrounding after
  // navigation start (which will be > soft navigation start_time). Nullopt if
  // never backgrounded. TimeDelta relative to (hard) navigation timeOrigin
  // (navigation start).
  std::optional<base::TimeDelta> first_background_time;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_DATA_H_
