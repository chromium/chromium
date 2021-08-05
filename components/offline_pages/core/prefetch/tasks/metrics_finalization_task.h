// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_METRICS_FINALIZATION_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_METRICS_FINALIZATION_TASK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchStore;

// Prefetching task that takes finished prefetch items, records interesting
// metrics about their final status, and marks them as zombies. Zombies are
// cleaned after a set period of time by the |StaleEntryFinalizerTask|.
// NOTE: this task is run periodically as reconciliation task or from some
// event handlers. It should not cause 'progress' in pipeline on which other
// tasks would depend. It should only move entries to ZOMBIE state.
class MetricsFinalizationTask : public Task {
 public:
  explicit MetricsFinalizationTask(PrefetchStore* prefetch_store);
  ~MetricsFinalizationTask() override;

 private:
  // Task implementation.
  void Run() override;
  void MetricsFinalized(bool result);

  PrefetchStore* prefetch_store_;

  base::WeakPtrFactory<MetricsFinalizationTask> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MetricsFinalizationTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_METRICS_FINALIZATION_TASK_H_
