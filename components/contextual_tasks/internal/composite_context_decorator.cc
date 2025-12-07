// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/composite_context_decorator.h"

#include <map>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/internal/fallback_title_context_decorator.h"
#include "components/contextual_tasks/internal/favicon_context_decorator.h"
#include "components/contextual_tasks/internal/history_context_decorator.h"
#include "components/contextual_tasks/internal/pending_context_decorator.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/history/core/browser/history_service.h"

namespace {
constexpr contextual_tasks::ContextualTaskContextSource kEarlyDecorators[] = {
    contextual_tasks::ContextualTaskContextSource::kPendingContextDecorator,
};

bool IsEarlyDecorator(contextual_tasks::ContextualTaskContextSource source) {
  return base::Contains(kEarlyDecorators, source);
}
}  // namespace

namespace contextual_tasks {

std::unique_ptr<CompositeContextDecorator> CreateCompositeContextDecorator(
    favicon::FaviconService* favicon_service,
    history::HistoryService* history_service,
    std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
        additional_decorators) {
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;
  decorators.emplace(ContextualTaskContextSource::kPendingContextDecorator,
                     std::make_unique<PendingContextDecorator>());
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
    std::unique_ptr<ContextDecorationParams> params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  std::vector<ContextDecorator*> decorators_to_run;

  // 1. Add "Early" decorators in the strict order defined by kEarlyDecorators.
  for (const auto& source : kEarlyDecorators) {
    // Check if we should run this decorator (either all are requested, or this
    // specific one is).
    if (sources.empty() || base::Contains(sources, source)) {
      auto it = decorators_.find(source);
      if (it != decorators_.end()) {
        decorators_to_run.push_back(it->second.get());
      }
    }
  }

  // 2. Add all "Other" decorators.
  for (const auto& pair : decorators_) {
    ContextualTaskContextSource source = pair.first;

    // Skip if this is an early decorator, as we've already handled it above.
    if (IsEarlyDecorator(source)) {
      continue;
    }

    if (sources.empty() || base::Contains(sources, source)) {
      decorators_to_run.push_back(pair.second.get());
    }
  }

  // Extract raw pointer before moving ownership. The pointer remains valid
  // because 'params' is moved into the final callback, which outlives the
  // decorator chain.
  ContextDecorationParams* params_ptr = params.get();

  // Wrap the context_callback with one that owns `params`. This makes the
  // params stay alive until the whole callback chain has finished.
  auto owning_final_callback = base::BindOnce(
      [](std::unique_ptr<ContextDecorationParams> owned_params,
         base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
             callback_to_run,
         std::unique_ptr<ContextualTaskContext> decorated_context) {
        // `owned_params` will be destroyed when this lambda goes out of scope
        // after callback_to_run is invoked.
        std::move(callback_to_run).Run(std::move(decorated_context));
      },
      std::move(params), std::move(context_callback));

  // Kicks off the decorator chain by calling RunNextDecorator with the first
  // decorator.
  RunNextDecorator(0, std::move(decorators_to_run), std::move(context),
                   params_ptr, std::move(owning_final_callback));
}

void CompositeContextDecorator::RunNextDecorator(
    size_t decorator_index,
    std::vector<ContextDecorator*> decorators_to_run,
    std::unique_ptr<ContextualTaskContext> context,
    ContextDecorationParams* params,
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
         ContextDecorationParams* params,
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
            std::move(decorated_context), params, std::move(final_callback));
      },
      weak_ptr_factory_.GetWeakPtr(), decorator_index + 1,
      std::move(decorators_to_run), std::move(final_callback), params);

  // Run the current decorator. When it's done, on_decorator_done_callback will
  // be called, which will in turn call RunNextDecorator for the next decorator.
  current_decorator->DecorateContext(std::move(context), params,
                                     std::move(on_decorator_done_callback));
}

}  // namespace contextual_tasks
