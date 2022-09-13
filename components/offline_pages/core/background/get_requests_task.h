// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_GET_REQUESTS_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_GET_REQUESTS_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class GetRequestsTask : public Task {
 public:
  GetRequestsTask(RequestQueueStore* store,
                  RequestQueueStore::GetRequestsCallback callback);

  GetRequestsTask(const GetRequestsTask&) = delete;
  GetRequestsTask& operator=(const GetRequestsTask&) = delete;

  ~GetRequestsTask() override;

 private:
  // Task implementation:
  void Run() override;
  // Step 1: Read the requests from he store.
  void ReadRequest();
  // Step 2: Calls the callback with result, completes the task.
  void CompleteWithResult(
      bool success,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Store from which requests will be read.
  raw_ptr<RequestQueueStore> store_;
  // Callback used to return the read results.
  RequestQueueStore::GetRequestsCallback callback_;

  base::WeakPtrFactory<GetRequestsTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_GET_REQUESTS_TASK_H_
