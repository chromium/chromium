// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REMOVE_REQUESTS_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REMOVE_REQUESTS_TASK_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class RemoveRequestsTask : public Task {
 public:
  RemoveRequestsTask(RequestQueueStore* store,
                     const std::vector<int64_t>& request_ids,
                     RequestQueueStore::UpdateCallback callback);

  RemoveRequestsTask(const RemoveRequestsTask&) = delete;
  RemoveRequestsTask& operator=(const RemoveRequestsTask&) = delete;

  ~RemoveRequestsTask() override;

 private:
  // TaskQueue::Task implementation.
  void Run() override;
  // Step 1. Removes requests from the store.
  void RemoveRequests();
  // Step for early termination, that builds failure result.
  void CompleteEarly(ItemActionStatus status);
  // Step 2. Processes update result, calls callback.
  void CompleteWithResult(UpdateRequestsResult result);

  // Store that this task updates.
  raw_ptr<RequestQueueStore> store_;
  // Request IDs to be updated.
  std::vector<int64_t> request_ids_;
  // Callback to complete the task.
  RequestQueueStore::UpdateCallback callback_;

  base::WeakPtrFactory<RemoveRequestsTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REMOVE_REQUESTS_TASK_H_
