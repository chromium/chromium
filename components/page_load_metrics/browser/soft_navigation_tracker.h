// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_

#include <deque>
#include <optional>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/interaction_to_next_paint_calculator.h"
#include "components/page_load_metrics/browser/layout_shift_normalization.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace page_load_metrics {

// SoftNavigationTracker is owned by PageLoadMetricsUpdateDispatcher.
// This is called when
// `PageLoadMetricsUpdateDispatcher::UpdateSoftNavigationMetrics` receives soft
// navigations from the renderer. It uses the soft navigation metrics records to
// slice the performance timeline. That is, in a loop triggered by the
// PageLoadMetrics.UpdateTiming IPC, it carries out the following steps:
//
// (1) It sends event timings, layout shifts, and LCP candidates that happened
// within a specific soft navigation's performance timeline, to their respective
// calculator objects (InteractionToNextPaintCalculator for event timings,
// LayoutShiftNormalization for layout shifts, and ContentfulPaint for LCP
// candidates).
//
// (2) It breaks if all soft navigations from the UpdateTiming call were
// processed. Otherwise, it notifies the observers about the latest
// soft navigation.
//
// (3) It resets the calculators.
//
// The loop is implemented in
// PageLoadMetricsUpdateDispatcher::UpdateSoftNavigationMetrics.
class SoftNavigationTracker {
 public:
  SoftNavigationTracker();
  ~SoftNavigationTracker();

  // Updates the metrics with the data from the renderer. This is called by
  // PageLoadMetricsUpdateDispatcher::UpdateSoftNavigationMetrics.
  // Returns true if and only if the |soft_navigation_metrics| is valid -
  // this method performs checks for data coming from the renderer.
  bool UpdateAndValidateMetrics(
      std::vector<mojom::SoftNavigationMetricsPtr> soft_navigation_metrics);

  // The current soft navigation. If there are no soft navigations, the
  // returned metrics will be empty; this can be detected by
  // checking the soft_navigation_offset field for 0.
  const mojom::SoftNavigationMetrics& current_soft_navigation() const {
    return *current_soft_navigation_;
  }

  // Returns the number of soft navigations that have seen by this tracker,
  // logging to UKM.
  size_t soft_navigation_count() const { return soft_navigation_count_; }

  // Returns true if there are any soft navigations that have not yet been
  // processed.
  bool HasNextSoftNavigation() const;

  // Advances to the next soft navigation that has not yet been processed.
  void AdvanceToNextSoftNavigation();

  // Processes the event timings, layout shifts, and LCP candidates in
  // the first argument that fall within the current soft navigation,
  // accumulating them in the given calculators.
  // Assumes that the measurements arrive in order.
  // After a Process call, the first argument is modified to only contain the
  // the metrics that have not yet been processed.
  // Returns the number of measurements that were processed.
  size_t Process(base::span<const mojom::EventTimingPtr>* event_timings,
               InteractionToNextPaintCalculator* calculator) const;
  size_t Process(base::span<const mojom::LayoutShiftPtr>* layout_shifts,
               LayoutShiftNormalization* layout_shift_normalization) const;
  size_t Process(
      base::span<const mojom::LargestContentfulPaintTimingPtr>* soft_lcps,
      ContentfulPaint* soft_lcp_candidate) const;

 private:
  bool ValidateIncoming(const mojom::SoftNavigationMetricsPtr& soft_navigation);
  // Returns the time that we use for slicing the performance timeline.
  base::TimeTicks SoftNavigationSlicingTime() const;

  uint64_t soft_navigation_count_ = 0;
  std::deque<mojom::SoftNavigationMetricsPtr> soft_navigations_to_process_;
  mojom::SoftNavigationMetricsPtr current_soft_navigation_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_
