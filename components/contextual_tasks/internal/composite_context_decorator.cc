// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/composite_context_decorator.h"

#include <map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/history/core/browser/history_service.h"
#include "fallback_title_context_decorator.h"
#include "favicon_context_decorator.h"
#include "history_context_decorator.h"

namespace contextual_tasks {

std::unique_ptr<CompositeContextDecorator> CreateCompositeContextDecorator(
    favicon::FaviconService* favicon_service,
    history::HistoryService* history_service,
    std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
        additional_decorators) {
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;
  decorators.emplace(ContextualTaskContextSource::kFallbackTitle,
                     std::make_unique<FallbackTitleContextDecorator>());
  decorators.emplace(
      ContextualTaskContextSource::kFaviconService,
      std::make_unique<FaviconContextDecorator>(favicon_service));
  decorators.emplace(
      ContextualTaskContextSource::kHistoryService,
      std::make_unique<HistoryContextDecorator>(history_service));

  for (auto& decorator : additional_decorators) {
    decorators.emplace(decorator.first, std::move(decorator.second));
  }

  return std::make_unique<CompositeContextDecorator>(std::move(decorators));
}

CompositeContextDecorator::CompositeContextDecorator(
    std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
        decorators)
    : decorators_(std::move(decorators)) {}

CompositeContextDecorator::~CompositeContextDecorator() = default;

void CompositeContextDecorator::DecorateContext(
    std::unique_ptr<ContextualTaskContext> context,
    const std::set<ContextualTaskContextSource>& sources,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  std::vector<ContextDecorator*> decorators_to_run;
  if (sources.empty()) {
    for (const auto& pair : decorators_) {
      decorators_to_run.push_back(pair.second.get());
    }
  } else {
    for (const auto& source : sources) {
      auto it = decorators_.find(source);
      if (it != decorators_.end()) {
        decorators_to_run.push_back(it->second.get());
      }
    }
  }

  // Kicks off the decorator chain by calling RunNextDecorator with the first
  // decorator.
  RunNextDecorator(0, std::move(decorators_to_run), std::move(context),
                   std::move(context_callback));
}

void CompositeContextDecorator::RunNextDecorator(
    size_t decorator_index,
    std::vector<ContextDecorator*> decorators_to_run,
    std::unique_ptr<ContextualTaskContext> context,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        final_callback) {
  // Base case for the recursion: if all decorators have been run, post the
  // final callback with the decorated context.
  if (decorator_index >= decorators_to_run.size()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(final_callback), std::move(context)));
    return;
  }

  // Get the current decorator to run.
  ContextDecorator* current_decorator = decorators_to_run[decorator_index];

  // Create a callback that will be invoked when the current decorator is done.
  // This callback will then trigger the next decorator in the chain.
  auto on_decorator_done_callback = base::BindOnce(
      // This lambda is the core of the chaining logic. It is executed when the
      // current decorator finishes.
      [](base::WeakPtr<CompositeContextDecorator> weak_self,
         size_t next_decorator_index,
         std::vector<ContextDecorator*> decorators_to_run,
         base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
             final_callback,
         std::unique_ptr<ContextualTaskContext> decorated_context) {
        // The weak pointer ensures that if the CompositeContextDecorator is
        // destroyed, the chain is safely terminated.
        if (!weak_self) {
          return;
        }
        // Continue the chain by calling RunNextDecorator for the next
        // decorator.
        weak_self->RunNextDecorator(
            next_decorator_index, std::move(decorators_to_run),
            std::move(decorated_context), std::move(final_callback));
      },
      weak_ptr_factory_.GetWeakPtr(), decorator_index + 1,
      std::move(decorators_to_run), std::move(final_callback));

  // Run the current decorator. When it's done, on_decorator_done_callback will
  // be called, which will in turn call RunNextDecorator for the next decorator.
  current_decorator->DecorateContext(std::move(context),
                                     std::move(on_decorator_done_callback));
}

}  // namespace contextual_tasks
