// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <map>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksServiceImpl::ContextualTasksServiceImpl() = default;

ContextualTasksServiceImpl::~ContextualTasksServiceImpl() {
  for (auto& observer : observers_) {
    observer.OnWillBeDestroyed();
  }
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

std::optional<ContextualTask> ContextualTasksServiceImpl::GetTaskById(
    const base::Uuid& task_id) const {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<ContextualTask> ContextualTasksServiceImpl::GetTasks() const {
  std::vector<ContextualTask> tasks;
  for (const auto& pair : tasks_) {
    tasks.push_back(pair.second);
  }
  return tasks;
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

void ContextualTasksServiceImpl::RemoveThreadFromTask(
    const base::Uuid& task_id,
    ThreadType type,
    const std::string& server_id) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.RemoveThread(type, server_id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                       weak_ptr_factory_.GetWeakPtr(), it->second,
                       TriggerSource::kLocal));
  }
}

void ContextualTasksServiceImpl::AttachUrlToTask(const base::Uuid& task_id,
                                                 const GURL& url) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.AddUrl(url);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksServiceImpl::NotifyTaskUpdated,
                       weak_ptr_factory_.GetWeakPtr(), it->second,
                       TriggerSource::kLocal));
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

void ContextualTasksServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

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
