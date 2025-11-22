// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/pending_context_decorator.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace contextual_tasks {

PendingContextDecorator::PendingContextDecorator() = default;
PendingContextDecorator::~PendingContextDecorator() = default;

void PendingContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(context_callback), std::move(context)));
}

}  // namespace contextual_tasks
