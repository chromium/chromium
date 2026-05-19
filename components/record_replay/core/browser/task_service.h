// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_SERVICE_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace record_replay {

class TaskData;
class TaskDefinition;

// Temporary type alias until TaskParameterValues is defined separately.
// Currently the values are the same as the TaskData.
using TaskParameterValues = TaskData;

// Service responsible for coordinating the lifecycle of automation tasks.
class TaskService : public KeyedService {
 public:
  TaskService();
  TaskService(const TaskService&) = delete;
  TaskService& operator=(const TaskService&) = delete;
  TaskService(TaskService&&) = delete;
  TaskService& operator=(TaskService&&) = delete;
  ~TaskService() override;

  // Triggered upon top-level navigations. This will check whether or not to
  // start observing a task. If necessary, it will create a TaskObserver.
  void OnURLVisited(const GURL& visited_url);

  // Triggered when we land on the task-end URL, receives a TaskDefinition
  // populated with TabParameterValues. The TaskObserver will receive this
  // as a callback and call it when it recognizes that the task ended.
  void OnTaskCompleted(const TaskDefinition& definition);

  // This will be given as a callback to the UI that offers task execution.
  void OnExecutionAccepted(const TaskDefinition& definition,
                           const TaskParameterValues& values);
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_SERVICE_H_
