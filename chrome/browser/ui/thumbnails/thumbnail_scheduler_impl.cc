// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_scheduler_impl.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"

// static
constexpr int ThumbnailSchedulerImpl::kMaxTotalCaptures;

// static
constexpr int ThumbnailSchedulerImpl::kMaxLowPriorityCaptures;

struct ThumbnailSchedulerImpl::TabSchedulingData {
  TabCapturePriority priority = TabCapturePriority::kNone;
};

struct ThumbnailSchedulerImpl::TabNode : public base::LinkNode<TabNode> {
  raw_ptr<TabCapturer> capturer = nullptr;
  TabSchedulingData data;
  bool is_capturing = false;
};

ThumbnailSchedulerImpl::ThumbnailSchedulerImpl(int max_total_captures,
                                               int max_low_priority_captures)
    : max_total_captures_(max_total_captures),
      max_low_priority_captures_(max_low_priority_captures) {
  DCHECK_GT(max_total_captures_, 0);
  DCHECK_GE(max_total_captures_, max_low_priority_captures_);
}

ThumbnailSchedulerImpl::~ThumbnailSchedulerImpl() = default;

void ThumbnailSchedulerImpl::AddTab(TabCapturer* tab) {
  auto result = tabs_.emplace(tab, TabNode{});
  DCHECK(result.second) << "tab added more than once";
  result.first->second.capturer = tab;
}

void ThumbnailSchedulerImpl::RemoveTab(TabCapturer* tab) {
  int num_removed = tabs_.erase(tab);
  DCHECK_EQ(1, num_removed) << "removed a tab that was never added";
}

void ThumbnailSchedulerImpl::SetTabCapturePriority(
    TabCapturer* tab,
    TabCapturePriority priority) {
  TabNode* const node = GetTabNode(tab);
  if (node->data.priority == priority)
    return;

  const TabSchedulingData old_data = node->data;
  node->data.priority = priority;
  Schedule(node, old_data);
}

void ThumbnailSchedulerImpl::Schedule(TabNode* tab_node,
                                      const TabSchedulingData& old_data) {
  // Scheduling is run after each tab priority change. So, at most one
  // tab may be scheduled, at most one tab may be descheduled, or both.
  TabNode* scheduled_tab = nullptr;
  TabNode* descheduled_tab = nullptr;

  // First, move the tab node to the correct list and update the
  // capturing counts.

  if (tab_node->next())
    tab_node->RemoveFromList();
  if (tab_node->is_capturing) {
    switch (old_data.priority) {
      case TabCapturePriority::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
      case TabCapturePriority::kLow:
        lo_prio_capture_count_ -= 1;
        break;
      case TabCapturePriority::kHigh:
        hi_prio_capture_count_ -= 1;
        break;
    }
  }

  switch (tab_node->data.priority) {
    case TabCapturePriority::kNone:
      descheduled_tab = tab_node;
      break;
    case TabCapturePriority::kLow:
      if (tab_node->is_capturing) {
        lo_prio_capturing_.Append(tab_node);
        lo_prio_capture_count_ += 1;
      } else {
        lo_prio_waiting_.Append(tab_node);
      }
      break;
    case TabCapturePriority::kHigh:
      if (tab_node->is_capturing) {
        hi_prio_capturing_.Append(tab_node);
        hi_prio_capture_count_ += 1;
      } else {
        hi_prio_waiting_.Append(tab_node);
      }
      break;
  }

  // First schedule a high priority tab if any are waiting, subject to
  // the maximum. Note that at most one may be schedulable. Prior to
  // this scheduling update, all possible high priority tabs were
  // scheduled. The only conditions leading to scheduling a new one are:
  //
  // 1. |tab| was newly prioritized and less than |max_total_captures_|
  //    tabs are capturing, or
  //
  // 2. |tab| was deprioritized and other tabs are waiting in
  //    |high_priority_wait_queue_|.
  //
  // So pick one tab from |high_priority_wait_queue_| and schedule it.
  // In case (1) this is guaranteed to be |tab|.

  if (hi_prio_capture_count_ < max_total_captures_ &&
      !hi_prio_waiting_.empty()) {
    scheduled_tab = hi_prio_waiting_.head()->value();
    scheduled_tab->RemoveFromList();
    hi_prio_capturing_.Append(scheduled_tab);
    hi_prio_capture_count_ += 1;
  }

  const int lo_prio_quota = std::min(
      max_low_priority_captures_, max_total_captures_ - hi_prio_capture_count_);

  // A high priority tab may have been deprioritized, so the low
  // priority quota may be off by one.
  DCHECK_LE(lo_prio_capture_count_, lo_prio_quota + 1);

  // A new high priority capture may preempt an existing low priority one. This
  // happens in two cases:
  //
  // 1. |tab| was set to high priority when at the max total captures, or
  //
  // 2. |tab| was changed from high to low priority while capturing and
  // there are other high priority tabs waiting.
  //
  // In this case, deschedule any low priority tab. Otherwise, schedule
  // a new one if below the quota. Note the latter cannot happen if a
  // high priority tab was scheduled above.
  if (lo_prio_capture_count_ > lo_prio_quota) {
    DCHECK_EQ(descheduled_tab, nullptr);
    descheduled_tab = lo_prio_capturing_.tail()->value();
    descheduled_tab->RemoveFromList();
    lo_prio_waiting_.Append(descheduled_tab);
    lo_prio_capture_count_ -= 1;
  } else if (lo_prio_capture_count_ < lo_prio_quota &&
             !lo_prio_waiting_.empty()) {
    DCHECK_EQ(scheduled_tab, nullptr);
    scheduled_tab = lo_prio_waiting_.head()->value();
    scheduled_tab->RemoveFromList();
    lo_prio_capturing_.Append(scheduled_tab);
    lo_prio_capture_count_ += 1;
  }

  if (descheduled_tab) {
    descheduled_tab->capturer->SetCapturePermittedByScheduler(false);
    descheduled_tab->is_capturing = false;
  }

  if (scheduled_tab) {
    scheduled_tab->capturer->SetCapturePermittedByScheduler(true);
    scheduled_tab->is_capturing = true;
  }
}

ThumbnailSchedulerImpl::TabNode* ThumbnailSchedulerImpl::GetTabNode(
    TabCapturer* tab) {
  auto it = tabs_.find(tab);
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130)
      << "referenced tab that is not registered with scheduler";
  return &it->second;
}
