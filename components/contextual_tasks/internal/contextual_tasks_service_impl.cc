// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <map>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"

namespace contextual_tasks {

ContextualTasksServiceImpl::ContextualTasksServiceImpl() = default;
ContextualTasksServiceImpl::~ContextualTasksServiceImpl() = default;

ContextualTask ContextualTasksServiceImpl::CreateTask() {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  tasks_.emplace(task_id, task);
  return task;
}

std::vector<ContextualTask> ContextualTasksServiceImpl::GetTasks() const {
  std::vector<ContextualTask> tasks;
  for (const auto& pair : tasks_) {
    tasks.push_back(pair.second);
  }
  return tasks;
}

void ContextualTasksServiceImpl::DeleteTask(const base::Uuid& task_id) {
  tasks_.erase(task_id);
}

}  // namespace contextual_tasks
