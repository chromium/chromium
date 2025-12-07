// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/favicon_context_decorator.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"

namespace contextual_tasks {

FaviconContextDecorator::FaviconContextDecorator(
    favicon::FaviconService* favicon_service)
    : favicon_service_(favicon_service) {}

FaviconContextDecorator::~FaviconContextDecorator() = default;

void FaviconContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  if (!favicon_service_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(context_callback), std::move(context)));
    return;
  }

  auto& attachments = GetMutableUrlAttachments(*context);
  if (attachments.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(context_callback), std::move(context)));
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(
      attachments.size(),
      base::BindOnce(&FaviconContextDecorator::OnAllFaviconsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     std::move(context_callback)));

  for (auto& attachment : attachments) {
    favicon_service_->GetFaviconImageForPageURL(
        attachment.GetURL(),
        base::BindOnce(&FaviconContextDecorator::OnFaviconImageReady,
                       weak_ptr_factory_.GetWeakPtr(), &attachment)
            .Then(barrier),
        &task_tracker_);
  }
}

void FaviconContextDecorator::OnFaviconImageReady(
    UrlAttachment* attachment,
    const favicon_base::FaviconImageResult& favicon_image_result) {
  if (!favicon_image_result.image.IsEmpty()) {
    auto& favicon_data =
        GetMutableUrlAttachmentDecoratorData(*attachment).favicon_data;
    favicon_data.image = favicon_image_result.image;
    favicon_data.icon_url = favicon_image_result.icon_url;
  }
}

void FaviconContextDecorator::OnAllFaviconsDone(
    std::unique_ptr<ContextualTaskContext> context,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  std::move(context_callback).Run(std::move(context));
}

}  // namespace contextual_tasks
