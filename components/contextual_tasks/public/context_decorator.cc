// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/context_decorator.h"

#include "components/contextual_tasks/public/contextual_task_context.h"

namespace contextual_tasks {

ContextDecorator::~ContextDecorator() = default;

std::vector<UrlAttachment>& ContextDecorator::GetMutableUrlAttachments(
    ContextualTaskContext& task) {
  return task.GetMutableUrlAttachments();
}

UrlAttachmentDecoratorData&
ContextDecorator::GetMutableUrlAttachmentDecoratorData(
    UrlAttachment& attachment) {
  return attachment.GetMutableDecoratorData();
}

}  // namespace contextual_tasks
