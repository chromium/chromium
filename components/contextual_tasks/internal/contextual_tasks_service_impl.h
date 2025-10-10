// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
#include "components/contextual_tasks/internal/ai_thread_sync_bridge.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/model/data_type_store.h"
#include "url/gurl.h"

namespace contextual_tasks {

class CompositeContextDecorator;
struct ContextualTaskContext;

class ContextualTasksServiceImpl : public ContextualTasksService,
                                   public AiThreadSyncBridge::Observer {
 public:
  ContextualTasksServiceImpl(
      version_info::Channel channel,
      syncer::OnceDataTypeStoreFactory data_type_store_factory,
      std::unique_ptr<CompositeContextDecorator> composite_context_decorator);
  ~ContextualTasksServiceImpl() override;

  ContextualTasksServiceImpl(const ContextualTasksServiceImpl&) = delete;
  ContextualTasksServiceImpl& operator=(const ContextualTasksServiceImpl&) =
      delete;

  // ContextualTasksService implementation.
  ContextualTask CreateTask() override;
  void GetTaskById(const base::Uuid& task_id,
                   base::OnceCallback<void(std::optional<ContextualTask>)>
                       callback) const override;
  void GetTasks(base::OnceCallback<void(std::vector<ContextualTask>)> callback)
      const override;
  void DeleteTask(const base::Uuid& task_id) override;
  void AddThreadToTask(const base::Uuid& task_id,
                       const Thread& thread) override;
  void RemoveThreadFromTask(const base::Uuid& task_id,
                            ThreadType type,
                            const std::string& server_id) override;
  void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) override;
  void DetachUrlFromTask(const base::Uuid& task_id, const GURL& url) override;
  void AttachSessionIdToTask(const base::Uuid& task_id,
                             SessionID session_id) override;
  void DetachSessionIdFromTask(const base::Uuid& task_id,
                               SessionID session_id) override;
  std::optional<ContextualTask> GetMostRecentContextualTaskForSessionID(
      SessionID session_id) const override;
  void GetContextForTask(
      const base::Uuid& task_id,
      const std::set<ContextualTaskContextSource>& sources,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;
  void AddObserver(ContextualTasksService::Observer* observer) override;
  void RemoveObserver(ContextualTasksService::Observer* observer) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAiThreadControllerDelegate() override;

  // AiThreadSyncBridge::Observer implementation.
  void OnThreadDataStoreLoaded() override;
  void OnThreadAddedOrUpdatedRemotely(
      const std::vector<Thread>& threads) override;
  void OnThreadRemovedRemotely(const std::vector<Thread>& threads) override;

  size_t GetSessionIdMapSizeForTesting() const;

 private:
  void NotifyTaskAdded(const ContextualTask& task, TriggerSource source);
  void NotifyTaskUpdated(const ContextualTask& task, TriggerSource source);
  void NotifyTaskRemoved(const base::Uuid& task_id, TriggerSource source);

  // The set of all tasks currently managed by the service, indexed by their
  // unique task ID for efficient lookup.
  std::map<base::Uuid, ContextualTask> tasks_;

  // A map from session IDs to task IDs, used to find the most recent task
  // associated with a given session.
  std::map<SessionID, base::Uuid> session_to_task_;

  // The entry point for the decorator chain that enriches the context.
  std::unique_ptr<CompositeContextDecorator> composite_context_decorator_;

  // Obsevers of the model.
  base::ObserverList<ContextualTasksService::Observer> observers_;

  std::unique_ptr<AiThreadSyncBridge> ai_thread_sync_bridge_;

  base::WeakPtrFactory<ContextualTasksServiceImpl> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_
