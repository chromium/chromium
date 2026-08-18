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

void SoftNavigationTracker::CompleteActiveNavigation() {
  // Complete the previously active navigation. Because an entry only becomes
  // `active_navigation_` upon receiving a valid commit (assigned below),
  // `active_navigation_->metrics` is guaranteed to be non-null.
  if (active_navigation_) {
    CHECK(active_navigation_->metrics);
    client_->OnSoftNavigationCompleted(*active_navigation_);
    active_navigation_.reset();
  }
}

bool SoftNavigationTracker::UpdateMainFrameMetrics(
    content::GlobalRenderFrameHostToken frame_token,
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigation_metrics,
    base::span<const mojom::EventTimingPtr> event_timings,
    base::span<const mojom::LayoutShiftPtr> layout_shifts,
    base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps) {
  // Add all new performance entries first. These entries are keyed by
  // performance timeline navigation ID and accumulate directly into the
  // active navigation or pending uncommitted buckets even if the soft
  // navigation commit has not arrived yet or fails validation.
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

  // Process incoming soft navigations in chronological order.
  // Each new commit completes the previously active navigation, promoting the
  // new navigation to active.
  for (auto& soft_navigation : soft_navigation_metrics) {
    CompleteActiveNavigation();

    uint64_t nav_id = soft_navigation->performance_timeline_navigation_id;

    // Prune uncommitted pending navigations with ID < nav_id.
    // These represent navigation IDs from performance entries where the soft
    // navigation commit was either canceled, aborted, or arrived after a
    // bfcache restore.
    while (!pending_navigations_.empty() &&
           pending_navigations_.begin()->first < nav_id) {
      pending_navigations_.erase(pending_navigations_.begin());
    }

    auto it = pending_navigations_.find(nav_id);
    if (it != pending_navigations_.end()) {
      active_navigation_ = std::move(it->second);
      pending_navigations_.erase(it);
    } else {
      active_navigation_ = std::make_unique<SoftNavigationData>();
    }
    active_navigation_->metrics = std::move(soft_navigation);
    ++soft_navigation_count_;
    client_->OnSoftNavigationCommit(*active_navigation_->metrics);
  }
  return true;
}

void SoftNavigationTracker::CompleteActiveNavigationAndFlush() {
  // Finalize the active navigation. Any uncommitted pending buckets in
  // `pending_navigations_` that never received a commit are cleared below
  // without notifying observers.
  CompleteActiveNavigation();
  pending_navigations_.clear();
}

SoftNavigationData* SoftNavigationTracker::GetOrCreateNavigation(
    uint64_t navigation_id) {
  if (navigation_id < kFirstSoftNavigationPerformanceTimelineNavigationId) {
    return nullptr;
  }
  if (active_navigation_) {
    CHECK(active_navigation_->metrics);
    uint64_t active_id =
        active_navigation_->metrics->performance_timeline_navigation_id;
    if (navigation_id == active_id) {
      return active_navigation_.get();
    }
    if (navigation_id < active_id) {
      // Belongs to a completed soft navigation that has already been dispatched
      // and pruned.
      return nullptr;
    }
  }
  auto it = pending_navigations_.find(navigation_id);
  if (it != pending_navigations_.end()) {
    return it->second.get();
  }
  if (pending_navigations_.size() >= kMaxSoftNavigations) {
    return nullptr;
  }
  return pending_navigations_
      .emplace(navigation_id, std::make_unique<SoftNavigationData>())
      .first->second.get();
}

const SoftNavigationData*
SoftNavigationTracker::GetSoftNavigationDataForTest(  // IN-TEST
    uint64_t navigation_id) const {
  if (active_navigation_ &&
      CHECK_DEREF(active_navigation_->metrics.get())
              .performance_timeline_navigation_id == navigation_id) {
    return active_navigation_.get();
  }
  auto it = pending_navigations_.find(navigation_id);
  return it != pending_navigations_.end() ? it->second.get() : nullptr;
}

bool SoftNavigationTracker::ValidateMetrics(
    const std::vector<mojom::SoftNavigationMetricsPtr>& soft_navigation_metrics)
    const {
  base::TimeTicks last_validated_slicing_time;
  base::UnguessableToken last_validated_token;
  uint64_t last_validated_id =
      kFirstSoftNavigationPerformanceTimelineNavigationId - 1;

  if (active_navigation_ && active_navigation_->metrics) {
    last_validated_id =
        active_navigation_->metrics->performance_timeline_navigation_id;
    last_validated_slicing_time =
        active_navigation_->metrics->soft_navigation_slicing_time;
    last_validated_token =
        active_navigation_->metrics->same_document_metrics_token;
  }

  for (const auto& soft_navigation : soft_navigation_metrics) {
    // TODO(crbug.com/490096674): Report invalid soft navigation metrics.
    if (soft_navigation->performance_timeline_navigation_id <=
            last_validated_id ||
        soft_navigation->start_time.is_zero() ||
        soft_navigation->soft_navigation_slicing_time.is_null() ||
        soft_navigation->same_document_metrics_token.is_empty()) {
      return false;
    }
    if (!last_validated_slicing_time.is_null() &&
        soft_navigation->soft_navigation_slicing_time <=
            last_validated_slicing_time) {
      return false;
    }
    if (!last_validated_token.is_empty() &&
        soft_navigation->same_document_metrics_token == last_validated_token) {
      return false;
    }
    last_validated_id = soft_navigation->performance_timeline_navigation_id;
    last_validated_slicing_time = soft_navigation->soft_navigation_slicing_time;
    last_validated_token = soft_navigation->same_document_metrics_token;
  }
  return true;
}

void SoftNavigationTracker::AddMainFrameEventTimings(
    content::GlobalRenderFrameHostToken frame_token,
    base::span<const mojom::EventTimingPtr> event_timings) {
  for (const auto& event : event_timings) {
    if (SoftNavigationData* nav =
            GetOrCreateNavigation(event->performance_timeline_navigation_id)) {
      nav->inp_calculator.AddNewEventTimings(frame_token,
                                             base::span_from_ref(event));
    }
  }
}

void SoftNavigationTracker::AddMainFrameLayoutShifts(
    base::span<const mojom::LayoutShiftPtr> layout_shifts) {
  base::TimeTicks now = base::TimeTicks::Now();
  for (const auto& shift : layout_shifts) {
    if (SoftNavigationData* nav =
            GetOrCreateNavigation(shift->performance_timeline_navigation_id)) {
      nav->cls_calculator.AddNewLayoutShifts(base::span_from_ref(shift), now);
    }
  }
}

void SoftNavigationTracker::AddMainFrameLargestContentfulPaints(
    base::span<const mojom::LargestContentfulPaintTimingPtr> soft_lcps) {
  for (const auto& lcp : soft_lcps) {
    if (SoftNavigationData* nav =
            GetOrCreateNavigation(lcp->performance_timeline_navigation_id)) {
      nav->lcp_handler.RecordMainFrameTiming(
          *lcp, /*first_input_or_scroll_notified_timestamp=*/std::nullopt);
    }
  }
}

}  // namespace page_load_metrics
