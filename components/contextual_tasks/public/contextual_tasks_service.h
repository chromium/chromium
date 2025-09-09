// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_

#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/keyed_service/core/keyed_service.h"

namespace contextual_tasks {

// Service that allows clients to create and manage contextual tasks.
// See `ContextualTask` for more information on what a task is.
class ContextualTasksService : public KeyedService {
 public:
  ContextualTasksService();
  ~ContextualTasksService() override;

  // Methods for creating and managing tasks.
  virtual ContextualTask CreateTask() = 0;
  virtual std::vector<ContextualTask> GetTasks() const = 0;
  virtual void DeleteTask(const base::Uuid& task_id) = 0;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
