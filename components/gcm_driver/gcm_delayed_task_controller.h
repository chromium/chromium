// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DELAYED_TASK_CONTROLLER_H_
#define COMPONENTS_GCM_DRIVER_GCM_DELAYED_TASK_CONTROLLER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"

namespace gcm {

// Helper class to save tasks to run until we're ready to execute them.
class GCMDelayedTaskController {
 public:
  GCMDelayedTaskController();
  ~GCMDelayedTaskController();

  // Adds a task that will be invoked once we're ready.
  void AddTask(base::OnceClosure task);

  // Sets ready status, which will release all of the pending tasks.
  void SetReady();

  // Returns true if it is ready to perform tasks.
  bool CanRunTaskWithoutDelay() const;

 private:
  void RunTasks();

  // Flag that indicates that controlled component is ready.
  bool ready_;

  std::vector<base::OnceClosure> delayed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(GCMDelayedTaskController);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DELAYED_TASK_CONTROLLER_H_
