// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_H_

namespace offline_pages {

// Interface for system-specific integrations with background task scheduling.
class PrefetchBackgroundTaskHandler {
 public:
  virtual ~PrefetchBackgroundTaskHandler() = default;

  // Stops any pending scheduled work.
  virtual void CancelBackgroundTask() = 0;

  // Ensures that Chrome will be started using a background task at an
  // appropriate time in the future.
  virtual void EnsureTaskScheduled() = 0;

  // Requests that the network backoff be increased due to a server response.
  virtual void Backoff() = 0;

  // Resets the backoff in case of a successful network attempt.
  virtual void ResetBackoff() = 0;

  // Do not use the backoff in rescheduling this time when the background task
  // is being killed due to system constraint. The backoff is still preserved
  // and will be used next time.
  virtual void PauseBackoffUntilNextRun() = 0;

  // Suspends scheduling the task for 1 day.
  virtual void Suspend() = 0;

  // Stops the suspension.
  virtual void RemoveSuspension() = 0;

  // Returns the number of seconds beyond the normal scheduling interval that
  // the backoff should wait.
  virtual int GetAdditionalBackoffSeconds() const = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_H_
