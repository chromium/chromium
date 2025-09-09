// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task.h"

#include "base/uuid.h"

namespace contextual_tasks {

ContextualTask::ContextualTask(const base::Uuid& task_id) : task_id_(task_id) {}
ContextualTask::~ContextualTask() = default;

ContextualTask::ContextualTask(const ContextualTask& other) = default;
ContextualTask::ContextualTask(ContextualTask&& other) = default;
ContextualTask& ContextualTask::operator=(const ContextualTask& other) =
    default;

const base::Uuid& ContextualTask::GetTaskId() const {
  return task_id_;
}

}  // namespace contextual_tasks
