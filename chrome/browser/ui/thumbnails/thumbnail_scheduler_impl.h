// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_SCHEDULER_IMPL_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_SCHEDULER_IMPL_H_

#include <deque>
#include <map>

#include "base/containers/linked_list.h"
#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"
#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"
#include "chrome/browser/ui/thumbnails/thumbnail_scheduler.h"

// A basic scheduler that given two limits, |max_total_captures| and
// |max_low_priority_captures|, ensures the following:
//
// - At most |max_total_captures| tabs capture simultaneously, and
// - At most |max_low_priority_captures| tabs with low priority
//   capture simultaneously.
//
// Tabs are only rescheduled in response to tab state changes.
class ThumbnailSchedulerImpl : public ThumbnailScheduler {
 public:
  static constexpr int kMaxTotalCaptures = 10;
  static constexpr int kMaxLowPriorityCaptures = 5;

  explicit ThumbnailSchedulerImpl(
      int max_total_captures = kMaxTotalCaptures,
      int max_low_priority_captures = kMaxLowPriorityCaptures);
  ~ThumbnailSchedulerImpl() override;

  // ThumbnailScheduler:
  void AddTab(TabCapturer* tab) override;
  void RemoveTab(TabCapturer* tab) override;
  void SetTabCapturePriority(TabCapturer* tab,
                             TabCapturePriority priority) override;

 private:
  // Contains all state that can influence scheduling decisions for a
  // tab.
  struct TabSchedulingData;

  // Stored in scheduling data structures.
  struct TabNode;

  // Runs scheduling after a change in |tab_node|'s scheduling data.
  // |old_data| is |tab_node|'s state before the change. Called after a
  // state change in any tab.
  void Schedule(TabNode* tab_node, const TabSchedulingData& old_data);

  TabNode* GetTabNode(TabCapturer* tab);

  const int max_total_captures_;
  const int max_low_priority_captures_;

  // Maps TabCapturer, the interface for a tab in the public API, to
  // TabNode, our internal representation of a tab. Each node is also in
  // at most one of the lists below, or none.
  std::map<TabCapturer*, TabNode> tabs_;

  // Queue of tabs that want to capture but haven't been scheduled yet.
  // One for each priority.
  base::LinkedList<TabNode> hi_prio_waiting_;
  base::LinkedList<TabNode> lo_prio_waiting_;

  // List of currently capturing tabs for each priority.
  base::LinkedList<TabNode> hi_prio_capturing_;
  base::LinkedList<TabNode> lo_prio_capturing_;

  // Number of tabs in each capture list (since base::LinkedList doesn't
  // track its size).
  int hi_prio_capture_count_ = 0;
  int lo_prio_capture_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_SCHEDULER_IMPL_H_
