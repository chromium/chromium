// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_

#include "base/uuid.h"

namespace contextual_tasks {

// A task is a representation of a user's journey to accomplish a goal. It
// could be a simple goal, like getting an answer to a question, or a complex
// multi-step process.
class ContextualTask {
 public:
  explicit ContextualTask(const base::Uuid& task_id);
  ~ContextualTask();

  ContextualTask(const ContextualTask& other);
  ContextualTask(ContextualTask&& other);
  ContextualTask& operator=(const ContextualTask& other);

  // Returns the unique ID of the task.
  const base::Uuid& GetTaskId() const;

 private:
  // The unique ID of the task.
  base::Uuid task_id_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_H_
