// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_SENT_GET_OPERATION_CLEANUP_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_SENT_GET_OPERATION_CLEANUP_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchNetworkRequestFactory;
class PrefetchStore;

// Reconciliation task responsible for cleaning up database entries that are in
// SENT_GET_OPERATION state.
class SentGetOperationCleanupTask : public Task {
 public:
  // Maximum number of attempts allowed for get operation request.
  static const int kMaxGetOperationAttempts;

  SentGetOperationCleanupTask(PrefetchStore* prefetch_store,
                              PrefetchNetworkRequestFactory* request_factory);

  SentGetOperationCleanupTask(const SentGetOperationCleanupTask&) = delete;
  SentGetOperationCleanupTask& operator=(const SentGetOperationCleanupTask&) =
      delete;

  ~SentGetOperationCleanupTask() override;

 private:
  void Run() override;
  void OnFinished(bool success);

  raw_ptr<PrefetchStore> prefetch_store_;  // Outlives this class.
  raw_ptr<PrefetchNetworkRequestFactory>
      request_factory_;  // Outlives this class.

  base::WeakPtrFactory<SentGetOperationCleanupTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_SENT_GET_OPERATION_CLEANUP_TASK_H_
