// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <map>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksServiceImpl::ContextualTasksServiceImpl() = default;
ContextualTasksServiceImpl::~ContextualTasksServiceImpl() = default;

ContextualTask ContextualTasksServiceImpl::CreateTask() {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto it = tasks_.emplace(task_id, ContextualTask(task_id)).first;
  return it->second;
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
}

void ContextualTasksServiceImpl::AssignServerIdToTask(
    const base::Uuid& task_id,
    ChatType type,
    const std::string& server_id) {
  auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    // Task not found, but we have a task ID. Create the task on the fly.
    it = tasks_.emplace(task_id, ContextualTask(task_id)).first;
  }
  it->second.AddChat(type, server_id);
}

void ContextualTasksServiceImpl::RemoveServerIdFromTask(
    const base::Uuid& task_id,
    ChatType type,
    const std::string& server_id) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.RemoveChat(type, server_id);
  }
}

void ContextualTasksServiceImpl::AttachUrlToTask(const base::Uuid& task_id,
                                                 const GURL& url) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.AddUrl(url);
  }
}

void ContextualTasksServiceImpl::DetachUrlFromTask(const base::Uuid& task_id,
                                                   const GURL& url) {
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    it->second.RemoveUrl(url);
  }
}

void ContextualTasksServiceImpl::AttachSessionIdToTask(
    const base::Uuid& task_id,
    SessionID session_id) {
  session_to_task_[session_id] = task_id;
  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
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

}  // namespace contextual_tasks
