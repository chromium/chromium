// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND____CORE_BACKGROUND_INITIALIZE_STORE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND____CORE_BACKGROUND_INITIALIZE_STORE_TASK_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

// Task performing a request queue store initialization and reset if one is
// necessary. Reset is triggered in case the initialization fails. If reset
// fails (perhaps not able to delete the folder or raze the database in case of
// SQLite-backed store) a new attempt can be made. Number of attempts is limited
// by |reset_attempts_left_|. Successful reset will be followed by another
// attempt to initialize the database.
//
// Task is completed when either the store is correctly initialized or there are
// not more reset attempts left.
class InitializeStoreTask : public Task {
 public:
  InitializeStoreTask(RequestQueueStore* store,
                      RequestQueueStore::InitializeCallback callback);
  ~InitializeStoreTask() override;

 private:
  // TaskQueue::Task implementation.
  void Run() override;
  // Step 1. Initialize store.
  void InitializeStore();
  // Step 2a. Completes initialization if successful or tries to reset if there
  // are more attempts left.
  void CompleteIfSuccessful(bool success);
  // Step 2b. Reset store in case initialization fails.
  void TryToResetStore();
  // Step 3. If 2b was successful go to step 1, else try 2b again.
  void OnStoreResetDone(bool success);

  // Store that this task initializes.
  RequestQueueStore* store_;
  // Number of attempts left to reset and reinitialize the store.
  int reset_attempts_left_;
  // Callback to complete the task.
  RequestQueueStore::InitializeCallback callback_;

  base::WeakPtrFactory<InitializeStoreTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(InitializeStoreTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND____CORE_BACKGROUND_INITIALIZE_STORE_TASK_H_
