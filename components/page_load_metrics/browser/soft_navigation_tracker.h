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
    virtual void OnSoftNavigationFirstContentfulPaint(
        const mojom::SoftNavigationMetrics& metrics) = 0;
    virtual void OnSoftNavigationCompleted(const SoftNavigationData& data) = 0;
  };

  // Performance timeline navigation ID for the first soft navigation
  // (hard navigation is 1).
  static constexpr uint64_t
      kFirstSoftNavigationPerformanceTimelineNavigationId = 2;
  // Maximum number of soft navigations to track to prevent
  // unbounded memory growth in case of corrupted renderer data.
  static constexpr size_t kMaxSoftNavigations = 100;

  explicit SoftNavigationTracker(Client* client);
  ~SoftNavigationTracker();

  // Updates the tracker with newly arrived main frame metrics.
  // Performs validation, updates per-navigation buckets, and pushes completed
  // and FCP navigation updates to `client_` in chronological order.
  // Returns true if all incoming soft navigations are valid.
  // TODO(crbug.com/494593459): Handle or taint reports on validation failure in
  // production.
  bool UpdateMainFrameMetrics(
      content::GlobalRenderFrameHostToken frame_token,
      base::span<const mojom::SoftNavigationMetricsPtr> soft_navigation_metrics,
      base::span<const mojom::EventTimingPtr> event_timings = {},
      base::span<const mojom::LayoutShiftPtr> layout_shifts = {},
      base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps = {});

  // Notifies the tracker that the page has become hidden, to record first
  // background time for active/open soft navigations. TimeDelta relative to
  // (hard) navigation timeOrigin (navigation start).
  void OnHidden(base::TimeDelta background_time);

  // Notifies the tracker that the page has become visible. TimeDelta relative
  // to (hard) navigation timeOrigin (navigation start).
  void OnShown(base::TimeDelta shown_time);

  // Updates the tracker with newly arrived subframe metrics.
  // Subframes do not participate directly in soft navigation heuristics and
  // thus do not have performance timeline navigation IDs; instead, subframe
  // events and layout shifts are attributed to the appropriate soft navigation
  // slice based on their timestamps.
  void UpdateSubFrameMetrics(
      content::GlobalRenderFrameHostToken frame_token,
      base::span<const mojom::EventTimingPtr> event_timings,
      base::span<const mojom::LayoutShiftPtr> layout_shifts);
  // Finalizes all active/in-progress soft navigations (e.g. on page destruction
  // or backgrounding) and pushes remaining completed navigations to `client_`.
  void CompleteActiveNavigationAndFlush();

  // Gets the SoftNavigationData for a specific navigation ID, or nullptr if not
  // tracked. Note: Returned pointer is only valid until the next mutating
  // operation on this tracker.
  SoftNavigationData* GetSoftNavigationDataForTest(
      uint64_t performance_timeline_navigation_id);
  const SoftNavigationData* GetSoftNavigationDataForTest(
      uint64_t performance_timeline_navigation_id) const;

  // Total count of soft navigations seen by this tracker.
  size_t soft_navigation_count() const { return soft_navigation_count_; }

 private:
  SoftNavigationData* GetSoftNavigationData(
      uint64_t performance_timeline_navigation_id);
  const SoftNavigationData* GetSoftNavigationData(
      uint64_t performance_timeline_navigation_id) const;

  // Finds the committed soft navigation slice that covers `timestamp` (i.e.
  // whose slicing time is the latest <= `timestamp`), or nullptr if `timestamp`
  // occurred before the first soft navigation or belongs to an already
  // dispatched navigation.
  SoftNavigationData* FindCommittedNavigationForTimestamp(
      base::TimeTicks timestamp);
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

  // Registers a committed soft navigation and updates tracking state.
  void AddMainFrameSoftNavigationCommit(
      const mojom::SoftNavigationMetrics& soft_navigation);

  // Adds or updates the main frame first contentful paint for an existing
  // soft navigation.
  void AddMainFrameFirstContentfulPaint(uint64_t navigation_id,
                                        base::TimeDelta first_contentful_paint);

  // Adds subframe event timings to their corresponding soft navigation slice
  // based on event->processing_start.
  void AddSubFrameEventTimings(
      content::GlobalRenderFrameHostToken frame_token,
      base::span<const mojom::EventTimingPtr> event_timings);

  // Adds subframe layout shifts to their corresponding soft navigation slice
  // based on shift->layout_shift_time.
  void AddSubFrameLayoutShifts(
      base::span<const mojom::LayoutShiftPtr> layout_shifts);

  // Returns true if `data` is non-null, has received a commit, and has an FCP
  // measurement.
  bool HasCommitAndFirstContentfulPaint(const SoftNavigationData* data) const;

  bool ValidateMetrics(base::span<const mojom::SoftNavigationMetricsPtr>
                           soft_navigation_metrics) const;
  void TryAdvanceAndDispatchSoftNavigationEvents();

  uint64_t soft_navigation_count_ = 0;
  uint64_t last_committed_navigation_id_ = 0;
  uint64_t last_reported_fcp_navigation_id_ = 0;
  raw_ptr<Client> client_ = nullptr;

  std::optional<base::TimeDelta> last_hidden_time_;
  std::optional<base::TimeDelta> last_shown_time_;

  // Map of all soft navigations currently tracked by this tracker, keyed by
  // performance_timeline_navigation_id (sorted in ascending/chronological
  // order).
  //
  // A navigation's state in this map is implicit:
  // - Awaiting FCP / Turn: `id > last_reported_fcp_navigation_id_` with
  //   `metrics->commit`
  // - Open (FCP Reported, Awaiting Next FCP):
  //   `id <= last_reported_fcp_navigation_id_`
  // - Dispatched: Erased from `navigations_` upon being reported to
  //   `OnSoftNavigationCompleted`.
  //
  // A completed navigation is only dispatched once:
  // 1. It has all of its own requisite data (commit metadata and FCP time).
  // 2. The subsequent navigation's FCP has arrived, which serves as a proxy
  //    to ensure sufficient time has elapsed to capture late INP, CLS, and LCP
  //    data for this navigation.
  //
  // The remaining active open navigation in this map is flushed upon
  // page unload / backgrounding in `CompleteActiveNavigationAndFlush()`.
  std::map<uint64_t, std::unique_ptr<SoftNavigationData>> navigations_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_SOFT_NAVIGATION_TRACKER_H_
