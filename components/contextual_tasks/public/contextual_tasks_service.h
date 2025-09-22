// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {

// Service that allows clients to create and manage contextual tasks.
// See `ContextualTask` for more information on what a task is.
class ContextualTasksService : public KeyedService {
 public:
  // Whether a task was updated by  a change in the local or remote client.
  enum class TriggerSource {
    kUnown,
    kLocal,
    kRemote,
  };

  // Observers observing updates to the ContextualTask data which can be
  // originated by either the local or remote clients.
  class Observer : public base::CheckedObserver {
   public:
    // The service is about to be destroyed. Ensures observers have a chance to
    // remove references before service destruction.
    virtual void OnWillBeDestroyed() {}

    // A new task was added at the given |source|.
    virtual void OnTaskAdded(const ContextualTask& task, TriggerSource source) {
    }

    // An existing task was updated at the given |source|.
    virtual void OnTaskUpdated(const ContextualTask& group,
                               TriggerSource source) {}

    // A task identifierd by `task_id` was removed.
    virtual void OnTaskRemoved(const base::Uuid& task_id,
                               TriggerSource source) {}
  };

  ContextualTasksService();
  ~ContextualTasksService() override;

  // Methods for creating and managing tasks.
  virtual ContextualTask CreateTask() = 0;
  virtual std::optional<ContextualTask> GetTaskById(
      const base::Uuid& task_id) const = 0;
  virtual std::vector<ContextualTask> GetTasks() const = 0;
  virtual void DeleteTask(const base::Uuid& task_id) = 0;

  // Methods related to server-side conversations.
  // When assigning a thread to a task_id that does not have a registered
  // task, the ContextualTask is created on the fly. We do not automatically
  // create tasks when removing threads.
  virtual void AddThreadToTask(const base::Uuid& task_id,
                               const Thread& thread) = 0;
  virtual void RemoveThreadFromTask(const base::Uuid& task_id,
                                    ThreadType type,
                                    const std::string& server_id) = 0;

  // Methods related to attaching URLs to tasks.
  virtual void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) = 0;
  virtual void DetachUrlFromTask(const base::Uuid& task_id,
                                 const GURL& url) = 0;

  // Methods related to attaching tabs to tasks using their SessionID.
  virtual void AttachSessionIdToTask(const base::Uuid& task_id,
                                     SessionID session_id) = 0;
  virtual void DetachSessionIdFromTask(const base::Uuid& task_id,
                                       SessionID session_id) = 0;
  virtual std::optional<ContextualTask> GetMostRecentContextualTaskForSessionID(
      SessionID session_id) const = 0;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
