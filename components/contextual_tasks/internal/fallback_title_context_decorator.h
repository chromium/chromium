// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_FALLBACK_TITLE_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_FALLBACK_TITLE_CONTEXT_DECORATOR_H_

#include "base/functional/callback.h"
#include "components/contextual_tasks/public/context_decorator.h"

namespace contextual_tasks {

struct ContextualTaskContext;
struct ContextDecorationParams;

// A decorator that enriches a context with a fallback title for URL
// attachments that do not have a title. The title is derived from the URL
// itself.
class FallbackTitleContextDecorator : public ContextDecorator {
 public:
  FallbackTitleContextDecorator();
  ~FallbackTitleContextDecorator() override;

  FallbackTitleContextDecorator(const FallbackTitleContextDecorator&) = delete;
  FallbackTitleContextDecorator& operator=(
      const FallbackTitleContextDecorator&) = delete;

  // ContextDecorator implementation:
  void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_FALLBACK_TITLE_CONTEXT_DECORATOR_H_
