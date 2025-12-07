// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_

#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {

enum class ThreadType {
  kUnknown,
  kAiMode,
};

// Represents a server-side conversation that is part of a `ContextualTask`.
struct Thread {
  Thread(ThreadType type,
         const std::string& server_id,
         const std::string& title,
         const std::string& conversation_turn_id);
  Thread(const Thread& other);
  ~Thread();

  // The type of conversation.
  ThreadType type;
  // The server-side ID of the conversation.
  std::string server_id;
  // Title of the thread that will be displayed to user.
  std::string title;

  // The unique server-side identifier for this specific conversation.
  // Since conversations can fork into a tree-like structure, this ID
  // represents a single path or branch within that tree.
  std::string conversation_turn_id;
};

struct UrlResource {
  UrlResource(const base::Uuid& url_id, const GURL& url);
  UrlResource(const UrlResource& other);
  ~UrlResource();

  // ID used for sync.
  base::Uuid url_id;

  // URL of the resource.
  GURL url;
};

// A task is a representation of a user's journey to accomplish a goal. It
// could be a simple goal, like getting an answer to a question, or a complex
// multi-step process. A task can have multiple pieces of context associated
// with it, such as URLs, session IDs, and ongoing server-side conversations.
class ContextualTask {
 public:
  explicit ContextualTask(const base::Uuid& task_id, bool is_ephemeral = false);
  ~ContextualTask();

  ContextualTask(const ContextualTask& other);
  ContextualTask(ContextualTask&& other);
  ContextualTask& operator=(const ContextualTask& other);

  // Returns the unique ID of the task.
  const base::Uuid& GetTaskId() const;

  // Whether the task is ephemeral. Ephemeral tasks aren't persisted.
  bool IsEphemeral() const { return is_ephemeral_; }

  // Sets the title of the task.
  void SetTitle(const std::string& title);

  // Gets the title of the task.
  std::string GetTitle() const;

  // Adds the server-side conversation to the task. If a task already has a
  // thread attached to it, it will be overwritten.
  void AddThread(const Thread& thread);

  // Removes the server-side conversation from the task.
  void RemoveThread(ThreadType type, const std::string& server_id);

  // Returns the server-side conversation associated with the task.
  std::optional<Thread> GetThread() const;

  // Adds a URL to the task. If the URL already exists, this method does
  // nothing and returns false. Otherwise, it will return true.
  bool AddUrlResource(const UrlResource& url_resource);

  // Returns the URLs relevant to the task.
  std::vector<UrlResource> GetUrlResources() const;

  // Removes a URL from the task. Returns the ID of the removed UrlResource if
  // found, otherwise returns std::nullopt.
  std::optional<base::Uuid> RemoveUrl(const GURL& url);

  // Returns the tab IDs of tabs related to the task.
  std::vector<SessionID> GetTabIds() const;

  // Adds a tab ID to the task. If the tab ID already exists, this method
  // does nothing.
  void AddTabId(SessionID tab_id);

  // Removes a tab ID from the task.
  void RemoveTabId(SessionID tab_id);

  // Clears all tab IDs associated with the task.
  void ClearTabIds();

 private:
  // The unique ID of the task.
  base::Uuid task_id_;

  // Whether the task is ephemeral. Ephemeral tasks are not persisted.
  bool is_ephemeral_ = false;

  // Title of the task;
  std::string title_;

  // The server-side conversation associated with the task.
  // When we persist this, we need to ensure we support up to N Threads.
  std::optional<Thread> thread_;

  // URLs relevant to the task.
  std::vector<UrlResource> url_resources_;

  // Tab IDs of tabs related to the task. Tab IDs are local to the device and
  // are not synced.
  std::vector<SessionID> tab_ids_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_
