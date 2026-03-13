// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/token_based_context_decorator.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

void TokenBasedContextDecorator::DecorateContext(
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

  auto tokens = GetTokensToDecorate(handle);

  DecorateContextWithTokens(std::move(context), controller, tokens,
                            std::move(context_callback));
}

void TokenBasedContextDecorator::DecorateContextWithTokens(
    std::unique_ptr<ContextualTaskContext> context,
    contextual_search::ContextualSearchContextController* controller,
    const std::vector<base::UnguessableToken>& tokens,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  for (const auto& token : tokens) {
    const auto* file_info = controller->GetFileInfo(token);
    if (!file_info || !file_info->tab_url.has_value()) {
      continue;
    }

    // Construct a new UrlAttachment for each FileInfo with a URL.
    UrlAttachment attachment(file_info->tab_url.value(),
                             ResourceType::kWebpage);
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

    bool is_duplicate = false;
    // This loop is O(N^2), but the number of attachments is expected to be
    // small, and using a set would make it harder to control which attachment
    // fields to use for deduplication.
    for (const auto& existing_attachment : context->GetUrlAttachments()) {
      if (existing_attachment.GetURL() == attachment.GetURL() &&
          existing_attachment.GetTitle() == attachment.GetTitle() &&
          existing_attachment.GetTabSessionId() ==
              attachment.GetTabSessionId()) {
        is_duplicate = true;
        break;
      }
    }

    if (is_duplicate) {
      continue;
    }

    // Extend the UrlAttachments with this new entry.
    GetMutableUrlAttachments(*context).push_back(std::move(attachment));
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(context_callback), std::move(context)));
}

}  // namespace contextual_tasks
