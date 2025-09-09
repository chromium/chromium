// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_

#include <map>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"

namespace contextual_tasks {

class ContextualTasksServiceImpl : public ContextualTasksService {
 public:
  ContextualTasksServiceImpl();
  ~ContextualTasksServiceImpl() override;

  // ContextualTasksService implementation.
  ContextualTask CreateTask() override;
  std::vector<ContextualTask> GetTasks() const override;
  void DeleteTask(const base::Uuid& task_id) override;
  void AssignServerIdToTask(const base::Uuid& task_id,
                            ChatType type,
                            const std::string& server_id) override;
  void RemoveServerIdFromTask(const base::Uuid& task_id,
                              ChatType type,
                              const std::string& server_id) override;

 private:
  // The set of all tasks currently managed by the service, indexed by their
  // unique task ID for efficient lookup.
  std::map<base::Uuid, ContextualTask> tasks_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_
