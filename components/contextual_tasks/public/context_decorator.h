// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXT_DECORATOR_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"

namespace favicon {
class FaviconService;
}

namespace history {
class HistoryService;
}

namespace contextual_tasks {

class CompositeContextDecorator;
class ContextDecorator;
struct ContextDecorationParams;
struct UrlAttachment;
struct UrlAttachmentDecoratorData;

// Factory function to create a CompositeContextDecorator pre-configured with a
// default set of ContextDecorators.
std::unique_ptr<CompositeContextDecorator> CreateCompositeContextDecorator(
    favicon::FaviconService* favicon_service,
    history::HistoryService* history_service,
    std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
        additional_decorators);

// Abstract interface for a decorator that enriches a ContextualTaskContext
// with additional metadata. The enrichment process is asynchronous.
class ContextDecorator {
 public:
  virtual ~ContextDecorator();

  // Asynchronously enriches the given |context|. Invokes |context_callback|
  // with the updated ContextualTaskContext on the original sequence when the
  // operation is complete, regardless of success or failure.
  // The parameter `params` may be `nullptr`.
  virtual void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) = 0;

 protected:
  // Provides subclasses with access to mutable URL attachments for a given
  // ContextualTaskContext.
  std::vector<UrlAttachment>& GetMutableUrlAttachments(
      ContextualTaskContext& task);

  // Provides subclasses with access to the decorator data block for a given
  // URL attachment.
  UrlAttachmentDecoratorData& GetMutableUrlAttachmentDecoratorData(
      UrlAttachment& attachment);
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXT_DECORATOR_H_
