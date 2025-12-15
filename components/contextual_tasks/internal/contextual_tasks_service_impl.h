// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
#include "components/contextual_tasks/internal/ai_thread_sync_bridge.h"
#include "components/contextual_tasks/internal/contextual_task_sync_bridge.h"
#include "components/contextual_tasks/internal/proto/ai_thread_entity.pb.h"
#include "components/contextual_tasks/internal/proto/contextual_task_entity.pb.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/model/data_type_store.h"
#include "url/gurl.h"

class AimEligibilityService;
class PrefService;

namespace signin {
class IdentityManager;
}

namespace contextual_tasks {

class CompositeContextDecorator;
struct ContextualTaskContext;
struct ContextDecorationParams;

class ContextualTasksServiceImpl : public ContextualTasksService,
                                   public AiThreadSyncBridge::Observer,
                                   public ContextualTaskSyncBridge::Observer {
 public:
  ContextualTasksServiceImpl(
      version_info::Channel channel,
      syncer::RepeatingDataTypeStoreFactory data_type_store_factory,
      std::unique_ptr<CompositeContextDecorator> composite_context_decorator,
      AimEligibilityService* aim_eligibility_service,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      bool supports_ephemeral_only);
  ~ContextualTasksServiceImpl() override;

  ContextualTasksServiceImpl(const ContextualTasksServiceImpl&) = delete;
  ContextualTasksServiceImpl& operator=(const ContextualTasksServiceImpl&) =
      delete;

  // ContextualTasksService implementation.
  FeatureEligibility GetFeatureEligibility() override;
  bool IsInitialized() override;
  ContextualTask CreateTask() override;
  ContextualTask CreateTaskFromUrl(const GURL& url) override;
  void GetTaskById(const base::Uuid& task_id,
                   base::OnceCallback<void(std::optional<ContextualTask>)>
                       callback) const override;
  void GetTasks(base::OnceCallback<void(std::vector<ContextualTask>)> callback)
      const override;
  void DeleteTask(const base::Uuid& task_id) override;
  void UpdateThreadForTask(const base::Uuid& task_id,
                           ThreadType thread_type,
                           const std::string& server_id,
                           std::optional<std::string> conversation_turn_id,
                           std::optional<std::string> title) override;
  void RemoveThreadFromTask(const base::Uuid& task_id,
                            ThreadType type,
                            const std::string& server_id) override;
  std::optional<ContextualTask> GetTaskFromServerId(
      ThreadType thread_type,
      const std::string& server_id) override;
  void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) override;
  void DetachUrlFromTask(const base::Uuid& task_id, const GURL& url) override;
  void SetUrlResourcesFromServer(
      const base::Uuid& task_id,
      std::vector<UrlResource> url_resources) override;
  void AssociateTabWithTask(const base::Uuid& task_id,
                            SessionID tab_id) override;
  void DisassociateTabFromTask(const base::Uuid& task_id,
                               SessionID tab_id) override;
  void DisassociateAllTabsFromTask(const base::Uuid& task_id) override;
  std::optional<ContextualTask> GetContextualTaskForTab(
      SessionID tab_id) const override;
  std::vector<SessionID> GetTabsAssociatedWithTask(
      const base::Uuid& tab_id) const override;
  void ClearAllTabAssociationsForTask(const base::Uuid& task_id) override;
  void GetContextForTask(
      const base::Uuid& task_id,
      const std::set<ContextualTaskContextSource>& sources,
      std::unique_ptr<ContextDecorationParams> params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;
  void AddObserver(ContextualTasksService::Observer* observer) override;
  void RemoveObserver(ContextualTasksService::Observer* observer) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAiThreadControllerDelegate() override;

  size_t GetTabIdMapSizeForTesting() const;

 private:
  friend class ContextualTasksServiceImplTest;

  // Finds a task by its ID, or creates a new one if it doesn't exist.
  // Returns an iterator to the task in the map and a boolean indicating whether
  // the task was newly created.
  std::pair<std::map<base::Uuid, ContextualTask>::iterator, bool>
  FindOrCreateTask(const base::Uuid& task_id,
                   ThreadType thread_type,
                   const std::string& server_id);

  void RemoveTaskInternal(const base::Uuid& task_id, TriggerSource source);

  void SetAiThreadSyncBridgeForTesting(
      std::unique_ptr<AiThreadSyncBridge> bridge);
  void SetContextualTaskSyncBridgeForTesting(
      std::unique_ptr<ContextualTaskSyncBridge> bridge);

  // AiThreadSyncBridge::Observer implementation.
  void OnThreadDataStoreLoaded() override;
  void OnThreadAddedOrUpdatedRemotely(
      const std::vector<proto::AiThreadEntity>& threads) override;
  void OnThreadRemovedRemotely(
      const std::vector<base::Uuid>& thread_ids) override;

  // ContextualTaskSyncBridge::Observer implementation.
  void OnContextualTaskDataStoreLoaded() override;
  void OnTaskAddedOrUpdatedRemotely(
      const std::vector<ContextualTask>& contextual_tasks) override;
  void OnTaskRemovedRemotely(const std::vector<base::Uuid>& task_ids) override;

  void NotifyTaskAdded(const ContextualTask& task, TriggerSource source);
  void NotifyTaskUpdated(const ContextualTask& task, TriggerSource source);
  void NotifyTaskRemoved(const base::Uuid& task_id, TriggerSource source);
  void NotifyTaskAssociatedToTab(const base::Uuid& task_id, SessionID tab_id);
  void NotifyTaskDisassociatedFromTab(const base::Uuid& task_id,
                                      SessionID tab_id);
  ContextualTask AddTaskAndNotify(ContextualTask task);

  void OnDataStoresLoaded();

  std::vector<ContextualTask> BuildTasks() const;

  // The set of all tasks currently managed by the service, indexed by their
  // unique task ID for efficient lookup.
  std::map<base::Uuid, ContextualTask> tasks_;

  // A map from tab IDs to task IDs, used to find the task associated with a
  // given tab.
  std::map<SessionID, base::Uuid> tab_to_task_;

  // The entry point for the decorator chain that enriches the context.
  std::unique_ptr<CompositeContextDecorator> composite_context_decorator_;

  // Obsevers of the model.
  base::ObserverList<ContextualTasksService::Observer> observers_;

  std::unique_ptr<AiThreadSyncBridge> ai_thread_sync_bridge_;
  std::unique_ptr<ContextualTaskSyncBridge> contextual_task_sync_bridge_;

  // Barrier to run OnDataStoresLoaded() after both sync bridges have loaded
  // their data.
  base::RepeatingClosure on_data_loaded_barrier_;

  // Whether the service is initialized.
  bool is_initialized_ = false;

  raw_ptr<AimEligibilityService> aim_eligibility_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ptr<PrefService> pref_service_;
  // Whether the service only supports ephemeral tasks.
  const bool supports_ephemeral_only_;

  base::WeakPtrFactory<ContextualTasksServiceImpl> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_CONTEXTUAL_TASKS_SERVICE_IMPL_H_
