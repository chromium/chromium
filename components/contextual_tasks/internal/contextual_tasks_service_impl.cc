// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/contextual_tasks/internal/account_utils.h"
#include "components/contextual_tasks/internal/composite_context_decorator.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksServiceImpl::ContextualTasksServiceImpl(
    version_info::Channel channel,
    syncer::RepeatingDataTypeStoreFactory data_type_store_factory,
    std::unique_ptr<CompositeContextDecorator> composite_context_decorator,
    AimEligibilityService* aim_eligibility_service,
    signin::IdentityManager* identity_manager,
    bool supports_ephemeral_only)
    : composite_context_decorator_(std::move(composite_context_decorator)),
      aim_eligibility_service_(aim_eligibility_service),
      identity_manager_(identity_manager),
      supports_ephemeral_only_(supports_ephemeral_only) {
  const base::RepeatingClosure& dump_stack =
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel);
  auto ai_thread_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::AI_THREAD, dump_stack);
  ai_thread_sync_bridge_ = std::make_unique<AiThreadSyncBridge>(
      std::move(ai_thread_processor), data_type_store_factory);
  auto contextual_task_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::CONTEXTUAL_TASK, dump_stack);
  contextual_task_sync_bridge_ = std::make_unique<ContextualTaskSyncBridge>(
      std::move(contextual_task_processor), data_type_store_factory);

  // Wait for both AiThreadSyncBridge and ContextualTaskSyncBridge to finish
  // loading their data store.
  on_data_loaded_barrier_ = base::BarrierClosure(
      2, base::BindOnce(&ContextualTasksServiceImpl::OnDataStoresLoaded,
                        weak_ptr_factory_.GetWeakPtr()));
}

ContextualTasksServiceImpl::~ContextualTasksServiceImpl() {
  for (auto& observer : observers_) {
    observer.OnWillBeDestroyed();
  }
}

FeatureEligibility ContextualTasksServiceImpl::GetFeatureEligibility() {
  return {base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks),
          aim_eligibility_service_->IsAimEligible()};
}

bool ContextualTasksServiceImpl::IsInitialized() {
  return is_initialized_;
}

ContextualTask ContextualTasksServiceImpl::CreateTask() {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id, supports_ephemeral_only_);
  return AddTaskAndNotify(std::move(task));
}

ContextualTask ContextualTasksServiceImpl::CreateTaskFromUrl(const GURL& url) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  bool is_ephemeral = supports_ephemeral_only_ ||
                      !IsUrlForPrimaryAccount(identity_manager_, url);
  ContextualTask task(task_id, is_ephemeral);
  return AddTaskAndNotify(std::move(task));
}

void ContextualTasksServiceImpl::GetTaskById(
    const base::Uuid& task_id,
    base::OnceCallback<void(std::optional<ContextualTask>)> callback) const {
  auto it = tasks_.find(task_id);
  std::optional<ContextualTask> result;
  if (it != tasks_.end()) {
    result = it->second;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void ContextualTasksServiceImpl::GetTasks(
    base::OnceCallback<void(std::vector<ContextualTask>)> callback) const {
  std::vector<ContextualTask> tasks;
  for (const auto& pair : tasks_) {
    ContextualTask task = pair.second;
    if (task.IsEphemeral() || supports_ephemeral_only_) {
      continue;
    }
    tasks.push_back(task);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(tasks)));
}

void ContextualTasksServiceImpl::DeleteTask(const base::Uuid& task_id) {
  contextual_task_sync_bridge_->OnTaskRemovedLocally(task_id);
  RemoveTaskInternal(task_id, TriggerSource::kLocal);
}

void ContextualTasksServiceImpl::AddThreadToTask(const base::Uuid& task_id,
                                                 const Thread& thread) {
  auto it = tasks_.find(task_id);
  bool is_new_task = (it == tasks_.end());
  if (is_new_task) {
    // Task not found, but we have a task ID. Create the task on the fly.
    it =
        tasks_
            .emplace(task_id, ContextualTask(task_id, supports_ephemeral_only_))
            .first;
  }

  it->second.AddThread(thread);

  if (is_new_task) {
    contextual_task_sync_bridge_->OnTaskAddedLocally(it->second);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskAdded,
                                  weak_ptr_factory_.GetWeakPtr(), it->second,
                                  TriggerSource::kLocal));
  } else {
    contextual_task_sync_bridge_->OnTaskUpdatedLocally(it->second);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                       weak_ptr_factory_.GetWeakPtr(), it->second,
                       TriggerSource::kLocal));
  }
}

void ContextualTasksServiceImpl::UpdateThreadTurnId(
    const base::Uuid& task_id,
    ThreadType thread_type,
    const std::string& server_id,
    const std::string& conversation_turn_id) {
  auto it = tasks_.find(task_id);
  bool is_new_task = (it == tasks_.end());
  if (is_new_task) {
    it =
        tasks_
            .emplace(task_id, ContextualTask(task_id, supports_ephemeral_only_))
            .first;
  }

  std::optional<Thread> thread = it->second.GetThread();
  if (thread.has_value() && thread->server_id != server_id) {
    return;
  }

  if (!thread.has_value()) {
    it->second.AddThread(
        Thread(thread_type, server_id, "", conversation_turn_id));
  } else {
    thread->conversation_turn_id = conversation_turn_id;
    it->second.AddThread(thread.value());
  }

  if (is_new_task) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskAdded,
                                  weak_ptr_factory_.GetWeakPtr(), it->second,
                                  TriggerSource::kLocal));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                       weak_ptr_factory_.GetWeakPtr(), it->second,
                       TriggerSource::kLocal));
  }
}

void ContextualTasksServiceImpl::RemoveThreadFromTask(
    const base::Uuid& task_id,
    ThreadType type,
    const std::string& server_id) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.RemoveThread(type, server_id);
    // If the task no longer has any thread, remove it.
    if (!it->second.GetThread()) {
      DeleteTask(task_id);
    }
  }
}

void ContextualTasksServiceImpl::AttachUrlToTask(const base::Uuid& task_id,
                                                 const GURL& url) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    UrlResource url_resource(base::Uuid::GenerateRandomV4(), url);
    if (it->second.AddUrlResource(url_resource)) {
      contextual_task_sync_bridge_->OnUrlAddedToTaskLocally(task_id,
                                                            url_resource);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                         weak_ptr_factory_.GetWeakPtr(), it->second,
                         TriggerSource::kLocal));
    }
  }
}

void ContextualTasksServiceImpl::DetachUrlFromTask(const base::Uuid& task_id,
                                                   const GURL& url) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    std::optional<base::Uuid> url_id = it->second.RemoveUrl(url);
    if (url_id) {
      contextual_task_sync_bridge_->OnUrlRemovedFromTaskLocally(url_id.value());
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                         weak_ptr_factory_.GetWeakPtr(), it->second,
                         TriggerSource::kLocal));
    }
  }
}

void ContextualTasksServiceImpl::AssociateTabWithTask(const base::Uuid& task_id,
                                                      SessionID tab_id) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    tab_to_task_[tab_id] = task_id;
    it->second.AddTabId(tab_id);
  }
}

void ContextualTasksServiceImpl::DisassociateTabFromTask(
    const base::Uuid& task_id,
    SessionID tab_id) {
  tab_to_task_.erase(tab_id);
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.RemoveTabId(tab_id);
  }
}

std::optional<ContextualTask>
ContextualTasksServiceImpl::GetContextualTaskForTab(SessionID tab_id) const {
  auto it = tab_to_task_.find(tab_id);
  if (it != tab_to_task_.end()) {
    auto task_it = tasks_.find(it->second);
    if (task_it != tasks_.end()) {
      return task_it->second;
    }
  }
  return std::nullopt;
}

void ContextualTasksServiceImpl::ClearAllTabAssociationsForTask(
    const base::Uuid& task_id) {
  auto task_it = tasks_.find(task_id);
  if (task_it == tasks_.end()) {
    return;
  }

  // Get a copy of the tab IDs before clearing them from the task.
  const std::vector<SessionID> tab_ids_to_remove = task_it->second.GetTabIds();

  // Clear the tab IDs from the task object itself.
  task_it->second.ClearTabIds();

  // Remove each of the tab IDs from the main lookup map.
  for (const auto& tab_id : tab_ids_to_remove) {
    tab_to_task_.erase(tab_id);
  }
}

void ContextualTasksServiceImpl::GetContextForTask(
    const base::Uuid& task_id,
    const std::set<ContextualTaskContextSource>& sources,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(context_callback),
                                  std::unique_ptr<ContextualTaskContext>()));
    return;
  }

  composite_context_decorator_->DecorateContext(
      std::make_unique<ContextualTaskContext>(it->second), sources,
      std::move(context_callback));
}

void ContextualTasksServiceImpl::AddObserver(
    ContextualTasksService::Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksServiceImpl::RemoveObserver(
    ContextualTasksService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ContextualTasksServiceImpl::GetAiThreadControllerDelegate() {
  return ai_thread_sync_bridge_->change_processor()->GetControllerDelegate();
}

void ContextualTasksServiceImpl::SetAiThreadSyncBridgeForTesting(
    std::unique_ptr<AiThreadSyncBridge> bridge) {
  ai_thread_sync_bridge_ = std::move(bridge);
}

void ContextualTasksServiceImpl::SetContextualTaskSyncBridgeForTesting(
    std::unique_ptr<ContextualTaskSyncBridge> bridge) {
  contextual_task_sync_bridge_ = std::move(bridge);
}

void ContextualTasksServiceImpl::OnThreadDataStoreLoaded() {
  on_data_loaded_barrier_.Run();
}

void ContextualTasksServiceImpl::OnThreadAddedOrUpdatedRemotely(
    const std::vector<proto::AiThreadEntity>& threads) {
  std::map<std::string, const proto::AiThreadEntity&> thread_map;
  for (const auto& thread : threads) {
    thread_map.emplace(thread.specifics().server_id(), thread);
  }

  for (auto& task_entry : tasks_) {
    ContextualTask& task = task_entry.second;
    if (!task.GetThread()) {
      continue;
    }

    auto it = thread_map.find(task.GetThread()->server_id);
    if (it == thread_map.end()) {
      continue;
    }

    // Check if the thread has changed for the task.
    const proto::AiThreadEntity& new_thread_entity = it->second;
    const std::optional<Thread>& old_thread = task.GetThread();
    if (old_thread->conversation_turn_id !=
            new_thread_entity.specifics().conversation_turn_id() ||
        old_thread->title != new_thread_entity.specifics().title()) {
      task.AddThread(
          Thread(ThreadType::kAiMode, new_thread_entity.specifics().server_id(),
                 new_thread_entity.specifics().title(),
                 new_thread_entity.specifics().conversation_turn_id()));
      NotifyTaskUpdated(task, TriggerSource::kRemote);
    }
  }
}

void ContextualTasksServiceImpl::OnThreadRemovedRemotely(
    const std::vector<base::Uuid>& thread_ids) {
  std::set<std::string> removed_thread_server_ids;
  for (const auto& id : thread_ids) {
    removed_thread_server_ids.insert(id.AsLowercaseString());
  }

  std::vector<base::Uuid> tasks_to_delete;
  for (const auto& task_entry : tasks_) {
    const ContextualTask& task = task_entry.second;
    if (task.GetThread()) {
      if (removed_thread_server_ids.count(task.GetThread()->server_id)) {
        tasks_to_delete.push_back(task.GetTaskId());
      }
    }
  }

  for (const auto& task_id : tasks_to_delete) {
    RemoveTaskInternal(task_id, TriggerSource::kRemote);
  }
}

void ContextualTasksServiceImpl::RemoveTaskInternal(const base::Uuid& task_id,
                                                    TriggerSource source) {
  auto task_it = tasks_.find(task_id);
  if (task_it == tasks_.end()) {
    return;
  }

  const auto& task = task_it->second;
  for (const auto& tab_id : task.GetTabIds()) {
    tab_to_task_.erase(tab_id);
  }

  tasks_.erase(task_it);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskRemoved,
                     weak_ptr_factory_.GetWeakPtr(), task_id, source));
}

size_t ContextualTasksServiceImpl::GetTabIdMapSizeForTesting() const {
  return tab_to_task_.size();
}

void ContextualTasksServiceImpl::OnContextualTaskDataStoreLoaded() {
  on_data_loaded_barrier_.Run();
  // TODO(shaktisahu): CHECK that no data read from store if
  // supports_ephemeral_only_.
}

void ContextualTasksServiceImpl::OnTaskAddedOrUpdatedRemotely(
    const std::vector<ContextualTask>& task_entities) {
  CHECK(!supports_ephemeral_only_);
}

void ContextualTasksServiceImpl::OnTaskRemovedRemotely(
    const std::vector<base::Uuid>& task_entities) {
  CHECK(!supports_ephemeral_only_);
}

void ContextualTasksServiceImpl::NotifyTaskAdded(const ContextualTask& task,
                                                 TriggerSource source) {
  for (auto& observer : observers_) {
    observer.OnTaskAdded(task, source);
  }
}

void ContextualTasksServiceImpl::NotifyTaskUpdated(const ContextualTask& task,
                                                   TriggerSource source) {
  for (auto& observer : observers_) {
    observer.OnTaskUpdated(task, source);
  }
}

void ContextualTasksServiceImpl::NotifyTaskRemoved(const base::Uuid& task_id,
                                                   TriggerSource source) {
  for (auto& observer : observers_) {
    observer.OnTaskRemoved(task_id, source);
  }
}

ContextualTask ContextualTasksServiceImpl::AddTaskAndNotify(
    ContextualTask task) {
  auto it = tasks_.emplace(task.GetTaskId(), task).first;
  contextual_task_sync_bridge_->OnTaskAddedLocally(task);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskAdded,
                                weak_ptr_factory_.GetWeakPtr(), it->second,
                                TriggerSource::kLocal));
  return it->second;
}

void ContextualTasksServiceImpl::OnDataStoresLoaded() {
  is_initialized_ = true;
  std::vector<ContextualTask> tasks = BuildTasks();
  for (const auto& task : tasks) {
    tasks_.emplace(task.GetTaskId(), task);
  }
  for (auto& observer : observers_) {
    observer.OnInitialized();
  }
}

std::vector<ContextualTask> ContextualTasksServiceImpl::BuildTasks() const {
  std::vector<ContextualTask> tasks = contextual_task_sync_bridge_->GetTasks();
  auto it = tasks.begin();
  while (it != tasks.end()) {
    // If the task doesn't have a thread, filter it out here as there is no
    // proper title to display it. It is also hard to differentiate between
    // tasks without threads. The caller should use GetTaskById() to retrieve
    // it.
    if (!it->GetThread()) {
      ++it;
      continue;
    }
    std::string thread_id = it->GetThread()->server_id;
    std::optional<Thread> thread = ai_thread_sync_bridge_->GetThread(thread_id);
    // Thread could be empty if the threads bridge is not fully synced, or if
    // the thread is deleted. In both cases we should not returning the task.
    // and should either wait for the sync update or delete the task.
    if (!thread) {
      it = tasks.erase(it);
    } else {
      it->AddThread(thread.value());
      ++it;
    }
  }
  return tasks;
}

}  // namespace contextual_tasks
