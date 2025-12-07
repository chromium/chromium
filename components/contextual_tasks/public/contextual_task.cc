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

ContextualTask::ContextualTask(const base::Uuid& task_id, bool is_ephemeral)
    : task_id_(task_id), is_ephemeral_(is_ephemeral) {}
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

std::optional<base::Uuid> ContextualTask::RemoveUrl(const GURL& url) {
  auto it =
      std::find_if(url_resources_.begin(), url_resources_.end(),
                   [&](const auto& resource) { return resource.url == url; });

  if (it != url_resources_.end()) {
    base::Uuid removed_id = it->url_id;
    url_resources_.erase(it);
    return removed_id;
  }

  return std::nullopt;
}

std::vector<SessionID> ContextualTask::GetTabIds() const {
  return tab_ids_;
}

void ContextualTask::AddTabId(SessionID tab_id) {
  if (std::find(tab_ids_.begin(), tab_ids_.end(), tab_id) == tab_ids_.end()) {
    tab_ids_.push_back(tab_id);
  }
}

void ContextualTask::RemoveTabId(SessionID tab_id) {
  std::erase(tab_ids_, tab_id);
}

void ContextualTask::ClearTabIds() {
  tab_ids_.clear();
}

}  // namespace contextual_tasks
