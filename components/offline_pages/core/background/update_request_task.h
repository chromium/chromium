// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_UPDATE_REQUEST_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_UPDATE_REQUEST_TASK_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

// Base class for update requests that only work on a single save page request.
// Derived classes should implement appropriate functionality by overloading
// |UpdateRequestImpl| method.
class UpdateRequestTask : public Task {
 public:
  UpdateRequestTask(RequestQueueStore* store,
                    int64_t request_id,
                    RequestQueueStore::UpdateCallback callback);
  ~UpdateRequestTask() override;

  // TaskQueue::Task implementation.
  void Run() override;

 protected:
  // Step 1. Reading the requests.
  void ReadRequest();
  // Step 2. Work is done in the implementation step.
  virtual void UpdateRequestImpl(UpdateRequestsResult result) = 0;
  // Step 3. Completes once update is done.
  void CompleteWithResult(UpdateRequestsResult result);

  // Function to uniformly validate read request call for store errors and
  // presence of the request.
  bool ValidateReadResult(const UpdateRequestsResult& result);

  RequestQueueStore* store() const { return store_; }

  int64_t request_id() const { return request_id_; }

  base::WeakPtr<UpdateRequestTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Store that this task updates. Not owned.
  RequestQueueStore* store_;
  // Request ID of the request to be started.
  int64_t request_id_;
  // Callback to complete the task.
  RequestQueueStore::UpdateCallback callback_;

  base::WeakPtrFactory<UpdateRequestTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(UpdateRequestTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_UPDATE_REQUEST_TASK_H_
