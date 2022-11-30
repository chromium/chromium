// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GENERATE_PAGE_BUNDLE_RECONCILE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GENERATE_PAGE_BUNDLE_RECONCILE_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchNetworkRequestFactory;
class PrefetchStore;

// Reconciling task that finds URL entries that claim they are being requested
// but for which there is no active network request and moves them either into
// state where network request can be retried or finishes them with error code
// if the number of retries is over the limit.
class GeneratePageBundleReconcileTask : public Task {
 public:
  static const int kMaxGenerateBundleAttempts;

  GeneratePageBundleReconcileTask(
      PrefetchStore* prefetch_store,
      PrefetchNetworkRequestFactory* request_factory);

  GeneratePageBundleReconcileTask(const GeneratePageBundleReconcileTask&) =
      delete;
  GeneratePageBundleReconcileTask& operator=(
      const GeneratePageBundleReconcileTask&) = delete;

  ~GeneratePageBundleReconcileTask() override;

 private:
  // Task implementation.
  void Run() override;
  void FinishedUpdate(bool success);

  raw_ptr<PrefetchStore> prefetch_store_;
  raw_ptr<PrefetchNetworkRequestFactory> request_factory_;

  base::WeakPtrFactory<GeneratePageBundleReconcileTask> weak_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GENERATE_PAGE_BUNDLE_RECONCILE_TASK_H_
