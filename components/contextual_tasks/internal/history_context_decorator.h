// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_HISTORY_CONTEXT_DECORATOR_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_HISTORY_CONTEXT_DECORATOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/history/core/browser/history_types.h"

namespace history {
class HistoryService;
}

namespace contextual_tasks {

struct ContextualTaskContext;
struct ContextDecorationParams;
struct UrlAttachment;

// A decorator that enriches a context with titles from history for URL
// attachments.
class HistoryContextDecorator : public ContextDecorator {
 public:
  explicit HistoryContextDecorator(history::HistoryService* history_service);
  ~HistoryContextDecorator() override;

  HistoryContextDecorator(const HistoryContextDecorator&) = delete;
  HistoryContextDecorator& operator=(const HistoryContextDecorator&) = delete;

  // ContextDecorator implementation:
  void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;

 private:
  void OnURLQueryComplete(UrlAttachment* attachment,
                          history::QueryURLResult result);

  void OnAllHistoryQueriesDone(
      std::unique_ptr<ContextualTaskContext> context,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback);

  raw_ptr<history::HistoryService> history_service_;
  base::CancelableTaskTracker task_tracker_;
  base::WeakPtrFactory<HistoryContextDecorator> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_HISTORY_CONTEXT_DECORATOR_H_
