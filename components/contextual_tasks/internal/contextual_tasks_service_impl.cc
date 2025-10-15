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
#include "components/contextual_tasks/internal/composite_context_decorator.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksServiceImpl::ContextualTasksServiceImpl(
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory data_type_store_factory,
    std::unique_ptr<CompositeContextDecorator> composite_context_decorator,
    AimEligibilityService* aim_eligibility_service)
    : composite_context_decorator_(std::move(composite_context_decorator)),
      aim_eligibility_service_(aim_eligibility_service) {
  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::AI_THREAD,
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel));
  ai_thread_sync_bridge_ = std::make_unique<AiThreadSyncBridge>(
      std::move(processor), std::move(data_type_store_factory));
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

ContextualTask ContextualTasksServiceImpl::CreateTask() {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto it = tasks_.emplace(task_id, ContextualTask(task_id)).first;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskAdded,
                                weak_ptr_factory_.GetWeakPtr(), it->second,
                                TriggerSource::kLocal));
  return it->second;
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
    tasks.push_back(pair.second);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(tasks)));
}

void ContextualTasksServiceImpl::DeleteTask(const base::Uuid& task_id) {
  auto task_it = tasks_.find(task_id);
  if (task_it == tasks_.end()) {
    return;
  }

  const auto& task = task_it->second;
  for (const auto& session_id : task.GetSessionIds()) {
    session_to_task_.erase(session_id);
  }

  tasks_.erase(task_it);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskRemoved,
                                weak_ptr_factory_.GetWeakPtr(), task_id,
                                TriggerSource::kLocal));
}

void ContextualTasksServiceImpl::AddThreadToTask(const base::Uuid& task_id,
                                                 const Thread& thread) {
  auto it = tasks_.find(task_id);
  bool is_new_task = (it == tasks_.end());
  if (is_new_task) {
    // Task not found, but we have a task ID. Create the task on the fly.
    it = tasks_.emplace(task_id, ContextualTask(task_id)).first;
  }

  it->second.AddThread(thread);

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

void ContextualTasksServiceImpl::UpdateThreadTurnId(
    const base::Uuid& task_id,
    ThreadType thread_type,
    const std::string& server_id,
    const std::string& conversation_turn_id) {
  auto it = tasks_.find(task_id);
  bool is_new_task = (it == tasks_.end());
  if (is_new_task) {
    it = tasks_.emplace(task_id, ContextualTask(task_id)).first;
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
    if (it->second.AddUrlResource(
            UrlResource(base::Uuid::GenerateRandomV4(), url))) {
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
    it->second.RemoveUrl(url);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                       weak_ptr_factory_.GetWeakPtr(), it->second,
                       TriggerSource::kLocal));
  }
}

void ContextualTasksServiceImpl::AttachSessionIdToTask(
    const base::Uuid& task_id,
    SessionID session_id) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    session_to_task_[session_id] = task_id;
    it->second.AddSessionId(session_id);
  }
}

void ContextualTasksServiceImpl::DetachSessionIdFromTask(
    const base::Uuid& task_id,
    SessionID session_id) {
  session_to_task_.erase(session_id);
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.RemoveSessionId(session_id);
  }
}

std::optional<ContextualTask>
ContextualTasksServiceImpl::GetMostRecentContextualTaskForSessionID(
    SessionID session_id) const {
  auto it = session_to_task_.find(session_id);
  if (it != session_to_task_.end()) {
    auto task_it = tasks_.find(it->second);
    if (task_it != tasks_.end()) {
      return task_it->second;
    }
  }
  return std::nullopt;
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

void ContextualTasksServiceImpl::OnThreadDataStoreLoaded() {}

void ContextualTasksServiceImpl::OnThreadAddedOrUpdatedRemotely(
    const std::vector<Thread>& threads) {}

void ContextualTasksServiceImpl::OnThreadRemovedRemotely(
    const std::vector<Thread>& threads) {}

size_t ContextualTasksServiceImpl::GetSessionIdMapSizeForTesting() const {
  return session_to_task_.size();
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

}  // namespace contextual_tasks
