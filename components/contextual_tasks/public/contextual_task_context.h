// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_CONTEXT_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_CONTEXT_H_

#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {

class ContextualTask;

// Represents a URL that is attached to a `ContextualTask`.
struct UrlAttachment {
  // The URL that is attached.
  GURL url;
};

// Represents the context associated with a `ContextualTask`. This is a
// snapshot of the context at a given point in time and is not kept in sync
// with the `ContextualTask`.
class ContextualTaskContext {
 public:
  // Constructs a `ContextualTaskContext` from a `ContextualTask`.
  explicit ContextualTaskContext(const ContextualTask& task);
  ~ContextualTaskContext();

  ContextualTaskContext(const ContextualTaskContext& other);
  ContextualTaskContext(ContextualTaskContext&& other);
  ContextualTaskContext& operator=(const ContextualTaskContext& other);
  ContextualTaskContext& operator=(ContextualTaskContext&& other);

  // Returns the unique ID of the task this context is for.
  const base::Uuid& GetTaskId() const;

  // Returns the URL attachments for the task.
  const std::vector<UrlAttachment>& GetUrlAttachments() const;

 private:
  // The unique ID of the task this context is for.
  base::Uuid task_id_;

  // The URL attachments for the task.
  std::vector<UrlAttachment> urls_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_CONTEXT_H_
