// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_UPDATE_SCHEDULER_H_
#define COMPONENTS_COMPONENT_UPDATER_UPDATE_SCHEDULER_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace component_updater {

// Abstract interface for an update task scheduler.
class UpdateScheduler {
 public:
  using OnFinishedCallback = base::OnceCallback<void()>;
  // Type of task to be run by the scheduler. The task can start asynchronous
  // operations and must call |on_finished| when all operations have completed.
  using UserTask =
      base::RepeatingCallback<void(OnFinishedCallback on_finished)>;
  using OnStopTaskCallback = base::RepeatingCallback<void()>;

  virtual ~UpdateScheduler() = default;

  // Schedules |user_task| to be run periodically with at least an interval of
  // |delay|. The first time |user_task| will be run after at least
  // |initial_delay|. If the execution of |user_task| must be stopped before it
  // called its |on_finished| callback, |on_stop| will be called.
  virtual void Schedule(base::TimeDelta initial_delay,
                        base::TimeDelta delay,
                        const UserTask& user_task,
                        const OnStopTaskCallback& on_stop) = 0;
  // Stops to periodically run |user_task| previously scheduled with |Schedule|.
  virtual void Stop() = 0;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_UPDATE_SCHEDULER_H_
