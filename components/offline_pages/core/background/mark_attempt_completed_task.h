// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_COMPLETED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_COMPLETED_TASK_H_

#include <stdint.h>
#include <memory>

#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/update_request_task.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class RequestQueueStore;

class MarkAttemptCompletedTask : public UpdateRequestTask {
 public:
  MarkAttemptCompletedTask(RequestQueueStore* store,
                           int64_t request_id,
                           FailState fail_state,
                           RequestQueueStore::UpdateCallback callback);
  ~MarkAttemptCompletedTask() override;

 protected:
  // UpdateRequestTask implementation:
  void UpdateRequestImpl(UpdateRequestsResult result) override;

 private:
  // Reason attempt failed, if any.
  FailState fail_state_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_COMPLETED_TASK_H_
