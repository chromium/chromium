// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_REFRESH_TASK_SCHEDULER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_REFRESH_TASK_SCHEDULER_H_

#include "base/time/time.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

// Schedules a background task for refreshing the Feed.
// When the scheduled task executes, it calls
// FeedStream::ExecuteRefreshTask(task_id).
class RefreshTaskScheduler {
 public:
  RefreshTaskScheduler() = default;
  virtual ~RefreshTaskScheduler() = default;

  // Schedules the task to run after |delay|. Overrides any previous schedule.
  virtual void EnsureScheduled(RefreshTaskId id, base::TimeDelta delay) = 0;
  // Cancel the task if it was previously scheduled.
  virtual void Cancel(RefreshTaskId id) = 0;
  // After FeedStream::ExecuteRefreshTask is called, the callee must call this
  // function to indicate the work is complete.
  virtual void RefreshTaskComplete(RefreshTaskId id) = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_REFRESH_TASK_SCHEDULER_H_
