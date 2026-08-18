// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/soft_navigation_data.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/global_routing_id.h"

namespace page_load_metrics {

// SoftNavigationTracker manages soft navigation metrics received from the
// renderer.
class SoftNavigationTracker {
 public:
  class Client {
   public:
    virtual ~Client() = default;
    virtual void OnSoftNavigationCommit(
        const mojom::SoftNavigationMetrics& metrics) = 0;
    virtual void OnSoftNavigationCompleted(const SoftNavigationData& data) = 0;
  };

  // Performance timeline navigation ID for the first soft navigation
  // (hard navigation is 1).
  static constexpr uint64_t
      kFirstSoftNavigationPerformanceTimelineNavigationId = 2;
  // Maximum number of uncommitted soft navigations to track to prevent
  // unbounded memory growth in case of corrupted renderer data.
  static constexpr size_t kMaxSoftNavigations = 100;

  explicit SoftNavigationTracker(Client* client);
  ~SoftNavigationTracker();

  // Updates the tracker with newly arrived main frame metrics.
  // Performs validation, updates per-navigation buckets, and pushes completed
  // and committed navigation updates to `client_` in chronological order.
  // Returns true if all incoming soft navigations are valid.
  // TODO(crbug.com/494593459): Handle or taint reports on validation failure in
  // production.
  bool UpdateMainFrameMetrics(
      content::GlobalRenderFrameHostToken frame_token,
      std::vector<mojom::SoftNavigationMetricsPtr> soft_navigation_metrics,
      base::span<const mojom::EventTimingPtr> event_timings = {},
      base::span<const mojom::LayoutShiftPtr> layout_shifts = {},
      base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps = {});

  // Finalizes all active/in-progress soft navigations (e.g. on page destruction
  // or backgrounding) and pushes remaining completed navigations to `client_`.
  void CompleteActiveNavigationAndFlush();

  // Gets the SoftNavigationData for a specific navigation ID, or nullptr if not
  // found.
  const SoftNavigationData* GetSoftNavigationDataForTest(
      uint64_t performance_timeline_navigation_id) const;

  // Total count of soft navigations seen by this tracker.
  size_t soft_navigation_count() const { return soft_navigation_count_; }

 private:
  void CompleteActiveNavigation();
  // Adds main frame event timings to their corresponding soft navigation based
  // on event->performance_timeline_navigation_id.
  void AddMainFrameEventTimings(
      content::GlobalRenderFrameHostToken frame_token,
      base::span<const mojom::EventTimingPtr> event_timings);

  // Adds main frame layout shifts to their corresponding soft navigation based
  // on shift->performance_timeline_navigation_id.
  void AddMainFrameLayoutShifts(
      base::span<const mojom::LayoutShiftPtr> layout_shifts);

  // Adds main frame soft LCP candidates to their corresponding soft navigation
  // based on lcp->performance_timeline_navigation_id.
  void AddMainFrameLargestContentfulPaints(
      base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps);

  SoftNavigationData* GetOrCreateNavigation(uint64_t navigation_id);
  bool ValidateMetrics(const std::vector<mojom::SoftNavigationMetricsPtr>&
                           soft_navigation_metrics) const;

  uint64_t soft_navigation_count_ = 0;
  raw_ptr<Client> client_ = nullptr;
  // The single active committed soft navigation currently accumulating metrics.
  std::unique_ptr<SoftNavigationData> active_navigation_;
  // Pre-allocated data buckets for performance entries that arrived before
  // their soft navigation commit IPC. A std::map is used so that pending
  // entries remain sorted by navigation_id for chronological pruning.
  std::map<uint64_t, std::unique_ptr<SoftNavigationData>> pending_navigations_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_
