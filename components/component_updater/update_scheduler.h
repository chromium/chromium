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
  // Type of task to be run by the scheduler.
  using UserTask = base::RepeatingClosure;

  virtual ~UpdateScheduler() = default;

  // Schedules `user_task` to run after at least `initial_delay`, then
  // repeatedly at intervals of at least `delay`.
  virtual void Schedule(base::TimeDelta initial_delay,
                        base::TimeDelta delay,
                        const UserTask& user_task) = 0;

  // Stops running the task passed to `Schedule()`.
  virtual void Stop() = 0;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_UPDATE_SCHEDULER_H_
