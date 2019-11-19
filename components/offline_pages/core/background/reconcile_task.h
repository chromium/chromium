// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_RECONCILE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_RECONCILE_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

// The reconcile task should be run at request coordinator startup to find any
// tasks that were offlining when chrome died, and change the state back to
// available.
class ReconcileTask : public Task {
 public:
  ReconcileTask(RequestQueueStore* store,
                RequestQueueStore::UpdateCallback callback);
  ~ReconcileTask() override;

  // TaskQueue::Task implementation:
  // Starts the async chain.
  void Run() override;

 private:
  // Step 1. Get results from the store.
  void GetRequests();

  // Step 2. Flip OFFLINING requests to AVAILABLE and put back in queue.
  void Reconcile(bool success,
                 std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Step 3. Processes update result.
  void UpdateCompleted(UpdateRequestsResult update_result);

  // Member variables, all pointers are not owned here.
  RequestQueueStore* store_;
  // Callback to complete the task.
  RequestQueueStore::UpdateCallback callback_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<ReconcileTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_RECONCILE_TASK_H_
