// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_

#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

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

  // Methods related to server-side conversations.
  // When assigning a server ID to a task_id that does not have a registered
  // task, the ContextualTask is created on the fly. We do not automatically
  // create tasks when removing server IDs.
  virtual void AssignServerIdToTask(const base::Uuid& task_id,
                                    ChatType type,
                                    const std::string& server_id) = 0;
  virtual void RemoveServerIdFromTask(const base::Uuid& task_id,
                                      ChatType type,
                                      const std::string& server_id) = 0;

  // Methods related to attaching URLs to tasks.
  virtual void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) = 0;
  virtual void DetachUrlFromTask(const base::Uuid& task_id,
                                 const GURL& url) = 0;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
