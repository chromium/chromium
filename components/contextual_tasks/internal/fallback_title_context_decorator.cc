// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/fallback_title_context_decorator.h"

#include <string>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

namespace {

// Max number of chars for the title of a URL.
const int kMaxTitleChars = 64;

// Formats a URL for display as a title when no title is available from history.
std::u16string GetTitleFromUrlForDisplay(const GURL& url) {
  std::u16string title = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitHTTPS,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  std::u16string display_title;
  if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

  // This will not be displayed in a security surface, so we can do the easy
  // elision from the trailing end instead of more complicated elision, or
  // restricting to only showing the hostname.
  gfx::ElideString(title, kMaxTitleChars, &display_title);
  return display_title;
}

}  // namespace

namespace contextual_tasks {

FallbackTitleContextDecorator::FallbackTitleContextDecorator() = default;

FallbackTitleContextDecorator::~FallbackTitleContextDecorator() = default;

void FallbackTitleContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  for (auto& attachment : GetMutableUrlAttachments(*context)) {
    GetMutableUrlAttachmentDecoratorData(attachment).fallback_title_data.title =
        GetTitleFromUrlForDisplay(attachment.GetURL());
  }

  // The operation was synchronous, but the ContextDecorator interface contract
  // requires the completion callback to be invoked asynchronously. We post a
  // task to the current thread's task runner to honor this contract.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(context_callback), std::move(context)));
}

}  // namespace contextual_tasks
