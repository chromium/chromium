// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/pending_context_decorator.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

PendingContextDecorator::PendingContextDecorator() = default;
PendingContextDecorator::~PendingContextDecorator() = default;

void PendingContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  if (!params || !params->contextual_search_session_handle) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(context_callback), std::move(context)));
    return;
  }

  auto* handle = params->contextual_search_session_handle.get();
  auto* controller = handle->GetController();
  if (!controller) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(context_callback), std::move(context)));
    return;
  }

  auto tokens = handle->GetUploadedContextTokens();
  auto submitted_tokens = handle->GetSubmittedContextTokens();
  tokens.insert(tokens.end(), submitted_tokens.begin(), submitted_tokens.end());
  for (const auto& token : tokens) {
    const auto* file_info = controller->GetFileInfo(token);
    if (!file_info || !file_info->tab_url.has_value()) {
      continue;
    }

    // Construct a new UrlAttachment for each FileInfo with a URL.
    UrlAttachment attachment(file_info->tab_url.value());
    auto& decorator_data = GetMutableUrlAttachmentDecoratorData(attachment);

    // Copy over the tab title if it exists.
    if (file_info->tab_title.has_value()) {
      decorator_data.contextual_search_context_data.title =
          base::UTF8ToUTF16(file_info->tab_title.value());
    }

    // Copy over the tab SessionID if it exists.
    if (file_info->tab_session_id.has_value()) {
      decorator_data.contextual_search_context_data.tab_session_id =
          file_info->tab_session_id.value();
    }

    // Extend the UrlAttachments with this new entry.
    GetMutableUrlAttachments(*context).push_back(std::move(attachment));
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(context_callback), std::move(context)));
}

}  // namespace contextual_tasks
