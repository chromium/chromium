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
               const std::string& title,
               const std::string& conversation_turn_id)
    : type(type),
      server_id(server_id),
      title(title),
      conversation_turn_id(conversation_turn_id) {}
Thread::Thread(const Thread& other) = default;
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

void ContextualTask::SetTitle(const std::string& title) {
  title_ = title;
}

std::string ContextualTask::GetTitle() const {
  return title_;
}

void ContextualTask::AddThread(const Thread& thread) {
  thread_ = thread;
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

UrlResource::UrlResource(const base::Uuid& url_id, const GURL& url)
    : url_id(url_id), url(url) {}
UrlResource::UrlResource(const UrlResource& other) = default;
UrlResource::~UrlResource() = default;

bool ContextualTask::AddUrlResource(const UrlResource& url_resource) {
  auto it = std::find_if(url_resources_.begin(), url_resources_.end(),
                         [&](const auto& existing_resource) {
                           return existing_resource.url == url_resource.url;
                         });
  if (it == url_resources_.end()) {
    url_resources_.push_back(url_resource);
    return true;
  }
  return false;
}

std::vector<UrlResource> ContextualTask::GetUrlResources() const {
  return url_resources_;
}

void ContextualTask::RemoveUrl(const GURL& url) {
  url_resources_.erase(
      std::remove_if(url_resources_.begin(), url_resources_.end(),
                     [&](const auto& resource) { return resource.url == url; }),
      url_resources_.end());
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
