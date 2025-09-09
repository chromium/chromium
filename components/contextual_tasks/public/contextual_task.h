// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_

#include <string>
#include <vector>

#include "base/uuid.h"
#include "url/gurl.h"

namespace contextual_tasks {

enum class ChatType {
  kAiMode,
};

// Represents a server-side conversation that is part of a `ContextualTask`.
struct Chat {
  Chat(ChatType type, const std::string& server_id);
  ~Chat();

  // The type of conversation.
  ChatType type;
  // The server-side ID of the conversation.
  std::string server_id;
};

// A task is a representation of a user's journey to accomplish a goal. It
// could be a simple goal, like getting an answer to a question, or a complex
// multi-step process. A task can have multiple pieces of context associated
// with it, such as URLs, session IDs, and ongoing server-side conversations.
class ContextualTask {
 public:
  explicit ContextualTask(const base::Uuid& task_id);
  ~ContextualTask();

  ContextualTask(const ContextualTask& other);
  ContextualTask(ContextualTask&& other);
  ContextualTask& operator=(const ContextualTask& other);

  // Returns the unique ID of the task.
  const base::Uuid& GetTaskId() const;

  // Adds the server-side conversation to the task. If a task already has a chat
  // attached to it, it will be overwritten.
  void AddChat(ChatType type, const std::string& server_id);

  // Removes the server-side conversation from the task.
  void RemoveChat(ChatType type, const std::string& server_id);

  // Returns the server-side conversation associated with the task.
  std::optional<Chat> GetChat() const;

  // Adds a URL to the task. If the URL already exists, this method does
  // nothing.
  void AddUrl(const GURL& url);

  // Returns the URLs relevant to the task.
  std::vector<GURL> GetUrls() const;

  // Removes a URL from the task.
  void RemoveUrl(const GURL& url);

 private:
  // The unique ID of the task.
  base::Uuid task_id_;

  // The server-side conversation associated with the task.
  // When we persist this, we need to ensure we support up to N Chats.
  std::optional<Chat> chat_;

  // URLs relevant to the task.
  std::vector<GURL> urls_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_
