// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task.h"

#include <string>
#include <vector>

#include "base/uuid.h"

namespace contextual_tasks {

Chat::Chat(ChatType type, const std::string& server_id)
    : type(type), server_id(server_id) {}
Chat::~Chat() = default;

ContextualTask::ContextualTask(const base::Uuid& task_id) : task_id_(task_id) {}
ContextualTask::~ContextualTask() = default;

ContextualTask::ContextualTask(const ContextualTask& other) = default;
ContextualTask::ContextualTask(ContextualTask&& other) = default;
ContextualTask& ContextualTask::operator=(const ContextualTask& other) =
    default;

const base::Uuid& ContextualTask::GetTaskId() const {
  return task_id_;
}

void ContextualTask::AddChat(ChatType type, const std::string& server_id) {
  chat_.emplace(type, server_id);
}

void ContextualTask::RemoveChat(ChatType type, const std::string& server_id) {
  if (chat_ && chat_->type == type && chat_->server_id == server_id) {
    chat_.reset();
  }
}

std::optional<Chat> ContextualTask::GetChat() const {
  return chat_;
}

}  // namespace contextual_tasks
