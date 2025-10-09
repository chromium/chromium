// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task_context.h"

#include "components/contextual_tasks/public/contextual_task.h"

namespace contextual_tasks {

UrlAttachment::UrlAttachment(const GURL& url) : url_(url) {}

UrlAttachment::~UrlAttachment() = default;

GURL UrlAttachment::GetURL() const {
  return url_;
}

std::u16string UrlAttachment::GetTitle() const {
  return decorator_data_.fallback_title_data.title;
}

UrlAttachmentDecoratorData& UrlAttachment::GetDecoratorData() {
  return decorator_data_;
}

ContextualTaskContext::ContextualTaskContext(const ContextualTask& task)
    : task_id_(task.GetTaskId()) {
  for (const auto& url_resource : task.GetUrlResources()) {
    urls_.emplace_back(url_resource.url);
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

std::vector<UrlAttachment>& ContextualTaskContext::GetMutableUrlAttachments() {
  return urls_;
}

}  // namespace contextual_tasks
