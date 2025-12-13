// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_COMPOSITE_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_COMPOSITE_CONTEXT_DECORATOR_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_task_context.h"

namespace contextual_tasks {
struct ContextDecorationParams;

// An implementation of ContextDecorator that serves as the entry point for a
// decorator chain. This class owns a map of concrete decorator
// implementations and runs a selection of them sequentially based on the
// `sources` parameter in `DecorateContext`.
//
// The decoration process is asynchronous, initiated by a call to
// DecorateContext. This kicks off a recursive-like chain of calls to
// RunNextDecorator, which executes each decorator in order. When a decorator
// completes, it invokes a callback that triggers the next decorator in the
// sequence. After all decorators have run, the final callback is invoked with
// the fully enriched context.
class CompositeContextDecorator {
 public:
  explicit CompositeContextDecorator(
      std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
          decorators);
  virtual ~CompositeContextDecorator();

  CompositeContextDecorator(const CompositeContextDecorator&) = delete;
  CompositeContextDecorator& operator=(const CompositeContextDecorator&) =
      delete;

  // Virtual for testing.
  virtual void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      const std::set<ContextualTaskContextSource>& sources,
      std::unique_ptr<ContextDecorationParams> params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback);

 private:
  // Recursively runs the next decorator in the chain.
  void RunNextDecorator(
      size_t decorator_index,
      std::vector<ContextDecorator*> decorators_to_run,
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          final_callback);

  // The ordered list of decorators to run.
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators_;

  // Weak pointer factory for safely posting tasks.
  base::WeakPtrFactory<CompositeContextDecorator> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_COMPOSITE_CONTEXT_DECORATOR_H_
