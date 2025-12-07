// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_PENDING_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_PENDING_CONTEXT_DECORATOR_H_

#include "components/contextual_tasks/public/context_decorator.h"

namespace contextual_tasks {
struct ContextDecorationParams;

class PendingContextDecorator : public ContextDecorator {
 public:
  PendingContextDecorator();
  ~PendingContextDecorator() override;

  // ContextDecorator:
  void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_PENDING_CONTEXT_DECORATOR_H_
