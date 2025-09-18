// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task.h"

#include <string>
#include <vector>

#include "base/uuid.h"
#include "url/gurl.h"

namespace contextual_tasks {

Thread::Thread(ThreadType type,
               const std::string& server_id,
               const std::string& title)
    : type(type), server_id(server_id), title(title) {}
Thread::~Thread() = default;

ContextualTask::ContextualTask(const base::Uuid& task_id) : task_id_(task_id) {}
ContextualTask::~ContextualTask() = default;

ContextualTask::ContextualTask(const ContextualTask& other) = default;
ContextualTask::ContextualTask(ContextualTask&& other) = default;
ContextualTask& ContextualTask::operator=(const ContextualTask& other) =
    default;

const base::Uuid& ContextualTask::GetTaskId() const {
  return task_id_;
}

void ContextualTask::AddThread(ThreadType type,
                               const std::string& server_id,
                               const std::string& title) {
  thread_.emplace(type, server_id, title);
}

void ContextualTask::RemoveThread(ThreadType type,
                                  const std::string& server_id) {
  if (thread_ && thread_->type == type && thread_->server_id == server_id) {
    thread_.reset();
  }
}

std::optional<Thread> ContextualTask::GetThread() const {
  return thread_;
}

void ContextualTask::AddUrl(const GURL& url) {
  if (std::find(urls_.begin(), urls_.end(), url) == urls_.end()) {
    urls_.push_back(url);
  }
}

std::vector<GURL> ContextualTask::GetUrls() const {
  return urls_;
}

void ContextualTask::RemoveUrl(const GURL& url) {
  urls_.erase(std::remove(urls_.begin(), urls_.end(), url), urls_.end());
}

std::vector<SessionID> ContextualTask::GetSessionIds() const {
  return session_ids_;
}

void ContextualTask::AddSessionId(SessionID session_id) {
  if (std::find(session_ids_.begin(), session_ids_.end(), session_id) ==
      session_ids_.end()) {
    session_ids_.push_back(session_id);
  }
}

void ContextualTask::RemoveSessionId(SessionID session_id) {
  session_ids_.erase(
      std::remove(session_ids_.begin(), session_ids_.end(), session_id),
      session_ids_.end());
}

}  // namespace contextual_tasks
