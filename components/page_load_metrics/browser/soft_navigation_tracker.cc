// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/soft_navigation_tracker.h"

#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "content/public/browser/global_routing_id.h"

namespace page_load_metrics {

SoftNavigationTracker::SoftNavigationTracker(Client* client) : client_(client) {
  CHECK(client_);
}

SoftNavigationTracker::~SoftNavigationTracker() = default;

bool SoftNavigationTracker::UpdateMainFrameMetrics(
    content::GlobalRenderFrameHostToken frame_token,
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigation_metrics,
    base::span<const mojom::EventTimingPtr> event_timings,
    base::span<const mojom::LayoutShiftPtr> layout_shifts,
    base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps) {
  // Add all new performance entries first. These entries are keyed by
  // performance timeline navigation ID and accumulate directly into the
  // corresponding navigation bucket in `navigations_`.
  AddMainFrameEventTimings(frame_token, event_timings);
  AddMainFrameLayoutShifts(layout_shifts);
  AddMainFrameLargestContentfulPaints(soft_lcps);

  if (soft_navigation_metrics.empty()) {
    return true;
  }

  // Validate soft navigation commit data specifically for consistency (e.g.
  // non-empty tokens, strictly increasing navigation IDs and slicing times).
  // If validation fails, we discard this part of the update only.
  if (!ValidateMetrics(soft_navigation_metrics)) {
    return false;
  }

  // Process incoming soft navigation updates. Commits arrive in strictly
  // increasing chronological order, while standalone data updates (such as FCP)
  // can arrive out of order for any tracked navigation.
  for (auto& soft_navigation : soft_navigation_metrics) {
    uint64_t nav_id = soft_navigation->performance_timeline_navigation_id;

    if (!soft_navigation->commit) {
      if (soft_navigation->first_contentful_paint.has_value()) {
        AddMainFrameFirstContentfulPaint(
            nav_id, *soft_navigation->first_contentful_paint);
      } else {
        // TODO(crbug.com/490096674): Report bad renderer if a message arrives
        // with neither commit nor FCP.
      }
      continue;
    }

    // Retrieves an uncommitted bucket created by earlier event timings or
    // layout shifts, or creates a new one for this soft navigation. This should
    // always succeed for valid commits unless the tracker has reached its
    // maximum capacity (`kMaxSoftNavigations`). Duplicate or out-of-order
    // commits are rejected by `ValidateMetrics`.
    SoftNavigationData* nav = GetOrCreateNavigationData(nav_id);
    if (!nav) {
      continue;
    }
    nav->metrics = std::move(soft_navigation);
    active_navigation_id_ = nav_id;
    ++soft_navigation_count_;
    client_->OnSoftNavigationCommit(*nav->metrics);
  }

  ProcessCompletedNavigationsAwaitingReportingCriteria();
  return true;
}

void SoftNavigationTracker::
    ProcessCompletedNavigationsAwaitingReportingCriteria() {
  PruneUncommittedNavigationsUpTo(active_navigation_id_);

  while (!navigations_.empty()) {
    auto it = navigations_.begin();
    uint64_t current_id = it->first;
    auto next_it = std::next(it);

    // Navigations only complete once they are no longer active, have their own
    // requisite commit and FCP data, and the subsequent navigation has also
    // committed and presented its FCP (which acts as a proxy ensuring
    // sufficient time has elapsed to capture late INP, CLS, and LCP data).
    if (current_id == active_navigation_id_ ||
        !HasCommitAndFirstContentfulPaint(it->second.get()) ||
        next_it == navigations_.end() ||
        !HasCommitAndFirstContentfulPaint(next_it->second.get())) {
      break;
    }

    auto ready_nav = std::move(it->second);
    navigations_.erase(it);
    client_->OnSoftNavigationCompleted(*ready_nav);
  }
}

void SoftNavigationTracker::CompleteActiveNavigationAndFlush() {
  // Drain and report all committed navigations in ascending navigation ID
  // order.
  // TODO(crbug.com/494593459): Investigate whether abandoned/in-flight commits
  // that were unloaded before presenting FCP should be reported to observers
  // with an optional FCP.
  for (const auto& [id, nav] : navigations_) {
    if (HasCommitAndFirstContentfulPaint(nav.get())) {
      client_->OnSoftNavigationCompleted(*nav);
    }
  }
  navigations_.clear();
}

SoftNavigationData* SoftNavigationTracker::GetOrCreateNavigationData(
    uint64_t navigation_id) {
  if (navigation_id < kFirstSoftNavigationPerformanceTimelineNavigationId) {
    return nullptr;
  }
  if (SoftNavigationData* nav = GetSoftNavigationData(navigation_id)) {
    return nav;
  }
  // If `navigation_id` is less than or equal to `active_navigation_id_` and not
  // found in `navigations_`, it belongs to a past soft navigation that has
  // already completed and been pruned.
  if (navigation_id <= active_navigation_id_) {
    return nullptr;
  }
  if (navigations_.size() >= kMaxSoftNavigations) {
    return nullptr;
  }
  return navigations_
      .emplace(navigation_id, std::make_unique<SoftNavigationData>())
      .first->second.get();
}

SoftNavigationData* SoftNavigationTracker::GetSoftNavigationDataForTest(
    uint64_t performance_timeline_navigation_id) {
  return GetSoftNavigationData(performance_timeline_navigation_id);
}

const SoftNavigationData* SoftNavigationTracker::GetSoftNavigationDataForTest(
    uint64_t performance_timeline_navigation_id) const {
  return GetSoftNavigationData(performance_timeline_navigation_id);
}

SoftNavigationData* SoftNavigationTracker::GetSoftNavigationData(
    uint64_t performance_timeline_navigation_id) {
  auto it = navigations_.find(performance_timeline_navigation_id);
  return it != navigations_.end() ? it->second.get() : nullptr;
}

const SoftNavigationData* SoftNavigationTracker::GetSoftNavigationData(
    uint64_t performance_timeline_navigation_id) const {
  auto it = navigations_.find(performance_timeline_navigation_id);
  return it != navigations_.end() ? it->second.get() : nullptr;
}

bool SoftNavigationTracker::ValidateMetrics(
    const std::vector<mojom::SoftNavigationMetricsPtr>& soft_navigation_metrics)
    const {
  base::TimeTicks last_validated_slicing_time;
  base::UnguessableToken last_validated_token;
  uint64_t last_validated_id =
      std::max(active_navigation_id_,
               kFirstSoftNavigationPerformanceTimelineNavigationId - 1);

  if (active_navigation_id_ != 0) {
    const SoftNavigationData* nav =
        GetSoftNavigationData(active_navigation_id_);
    if (nav && nav->metrics && nav->metrics->commit) {
      last_validated_slicing_time =
          nav->metrics->commit->soft_navigation_slicing_time;
      last_validated_token = nav->metrics->commit->same_document_metrics_token;
    }
  }

  for (const auto& soft_navigation : soft_navigation_metrics) {
    // TODO(crbug.com/490096674): Report invalid soft navigation metrics.
    if (!soft_navigation ||
        soft_navigation->performance_timeline_navigation_id <
            kFirstSoftNavigationPerformanceTimelineNavigationId) {
      return false;
    }

    // Standalone metric updates (such as FCP updates) can arrive out of order
    // for any tracked navigation, but must contain valid metric data.
    if (!soft_navigation->commit) {
      if (!soft_navigation->first_contentful_paint.has_value()) {
        return false;
      }
      continue;
    }

    // Commit messages must arrive in strictly increasing chronological order
    // and contain valid non-repeating commit metadata.
    const auto& commit = *soft_navigation->commit;
    if (soft_navigation->performance_timeline_navigation_id <=
            last_validated_id ||
        commit.start_time.is_zero() ||
        commit.soft_navigation_slicing_time.is_null() ||
        commit.same_document_metrics_token.is_empty()) {
      return false;
    }
    if (!last_validated_slicing_time.is_null() &&
        commit.soft_navigation_slicing_time <= last_validated_slicing_time) {
      return false;
    }
    if (!last_validated_token.is_empty() &&
        commit.same_document_metrics_token == last_validated_token) {
      return false;
    }
    last_validated_id = soft_navigation->performance_timeline_navigation_id;
    last_validated_slicing_time = commit.soft_navigation_slicing_time;
    last_validated_token = commit.same_document_metrics_token;
  }
  return true;
}

void SoftNavigationTracker::AddMainFrameEventTimings(
    content::GlobalRenderFrameHostToken frame_token,
    base::span<const mojom::EventTimingPtr> event_timings) {
  for (const auto& event : event_timings) {
    if (SoftNavigationData* nav = GetOrCreateNavigationData(
            event->performance_timeline_navigation_id)) {
      nav->inp_calculator.AddNewEventTimings(frame_token,
                                             base::span_from_ref(event));
    }
  }
}

void SoftNavigationTracker::AddMainFrameLayoutShifts(
    base::span<const mojom::LayoutShiftPtr> layout_shifts) {
  base::TimeTicks now = base::TimeTicks::Now();
  for (const auto& shift : layout_shifts) {
    if (SoftNavigationData* nav = GetOrCreateNavigationData(
            shift->performance_timeline_navigation_id)) {
      nav->cls_calculator.AddNewLayoutShifts(base::span_from_ref(shift), now);
    }
  }
}

void SoftNavigationTracker::AddMainFrameLargestContentfulPaints(
    base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps) {
  for (const auto& lcp : soft_lcps) {
    if (SoftNavigationData* nav = GetOrCreateNavigationData(
            lcp->performance_timeline_navigation_id)) {
      nav->lcp_handler.RecordMainFrameTiming(
          *lcp, /*first_input_or_scroll_notified_timestamp=*/std::nullopt);
    }
  }
}

void SoftNavigationTracker::AddMainFrameFirstContentfulPaint(
    uint64_t navigation_id,
    base::TimeDelta first_contentful_paint) {
  // Standalone metric update (e.g. FCP) for an existing tracked soft
  // navigation. Commits are always sent before FCP; if no committed navigation
  // entry exists (e.g. flushed due to bfcache or invalid renderer state),
  // ignore this late update rather than creating an uncommitted bucket.
  SoftNavigationData* nav = GetSoftNavigationData(navigation_id);
  if (nav && nav->metrics && nav->metrics->commit) {
    nav->metrics->first_contentful_paint = first_contentful_paint;
  }
}

void SoftNavigationTracker::PruneUncommittedNavigationsUpTo(
    uint64_t navigation_id) {
  // In a well-behaved renderer, commits arrive in strictly increasing
  // chronological order, so there should never be uncommitted navigation
  // buckets with a lower ID than `navigation_id`. However, this cleans up any
  // orphaned navigation IDs from performance entries where the soft navigation
  // commit was canceled, aborted, or arrived after a bfcache restore.
  for (auto it = navigations_.begin();
       it != navigations_.end() && it->first < navigation_id;) {
    if (!it->second->metrics || !it->second->metrics->commit) {
      it = navigations_.erase(it);
    } else {
      ++it;
    }
  }
}

bool SoftNavigationTracker::HasCommitAndFirstContentfulPaint(
    const SoftNavigationData* data) const {
  return data && data->metrics && data->metrics->commit &&
         data->metrics->first_contentful_paint.has_value();
}

}  // namespace page_load_metrics
