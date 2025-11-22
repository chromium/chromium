// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/history_context_decorator.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/history/core/browser/history_service.h"

namespace contextual_tasks {

HistoryContextDecorator::HistoryContextDecorator(
    history::HistoryService* history_service)
    : history_service_(history_service) {}

HistoryContextDecorator::~HistoryContextDecorator() = default;

void HistoryContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  if (!history_service_) {
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
      base::BindOnce(&HistoryContextDecorator::OnAllHistoryQueriesDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     std::move(context_callback)));

  for (auto& attachment : attachments) {
    history_service_->QueryURL(
        attachment.GetURL(),
        base::BindOnce(&HistoryContextDecorator::OnURLQueryComplete,
                       weak_ptr_factory_.GetWeakPtr(), &attachment)
            .Then(barrier),
        &task_tracker_);
  }
}

void HistoryContextDecorator::OnURLQueryComplete(
    UrlAttachment* attachment,
    history::QueryURLResult result) {
  if (result.success && !result.row.title().empty()) {
    GetMutableUrlAttachmentDecoratorData(*attachment).history_data.title =
        result.row.title();
  }
}

void HistoryContextDecorator::OnAllHistoryQueriesDone(
    std::unique_ptr<ContextualTaskContext> context,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  std::move(context_callback).Run(std::move(context));
}

}  // namespace contextual_tasks
