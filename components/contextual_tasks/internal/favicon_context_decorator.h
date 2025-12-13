// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_FAVICON_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_FAVICON_CONTEXT_DECORATOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/favicon_base/favicon_types.h"

namespace favicon {
class FaviconService;
}

namespace contextual_tasks {

struct ContextualTaskContext;
struct ContextDecorationParams;
struct UrlAttachment;

// A decorator that enriches a context with favicons for URL attachments.
class FaviconContextDecorator : public ContextDecorator {
 public:
  explicit FaviconContextDecorator(favicon::FaviconService* favicon_service);
  ~FaviconContextDecorator() override;

  FaviconContextDecorator(const FaviconContextDecorator&) = delete;
  FaviconContextDecorator& operator=(const FaviconContextDecorator&) = delete;

  // ContextDecorator implementation:
  void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;

 private:
  void OnFaviconImageReady(
      UrlAttachment* attachment,
      const favicon_base::FaviconImageResult& favicon_image_result);

  void OnAllFaviconsDone(
      std::unique_ptr<ContextualTaskContext> context,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback);

  raw_ptr<favicon::FaviconService> favicon_service_;
  base::CancelableTaskTracker task_tracker_;
  base::WeakPtrFactory<FaviconContextDecorator> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_FAVICON_CONTEXT_DECORATOR_H_
