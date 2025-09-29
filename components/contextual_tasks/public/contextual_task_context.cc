// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task_context.h"

#include "components/contextual_tasks/public/contextual_task.h"

namespace contextual_tasks {

ContextualTaskContext::ContextualTaskContext(const ContextualTask& task)
    : task_id_(task.GetTaskId()) {
  for (const auto& url : task.GetUrls()) {
    urls_.push_back({url});
  }
}

ContextualTaskContext::~ContextualTaskContext() = default;

ContextualTaskContext::ContextualTaskContext(
    const ContextualTaskContext& other) = default;

ContextualTaskContext::ContextualTaskContext(ContextualTaskContext&& other) =
    default;

ContextualTaskContext& ContextualTaskContext::operator=(
    const ContextualTaskContext& other) = default;

ContextualTaskContext& ContextualTaskContext::operator=(
    ContextualTaskContext&& other) = default;

const base::Uuid& ContextualTaskContext::GetTaskId() const {
  return task_id_;
}

const std::vector<UrlAttachment>& ContextualTaskContext::GetUrlAttachments()
    const {
  return urls_;
}

}  // namespace contextual_tasks
