// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_DEFERRED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_DEFERRED_TASK_H_

#include <stdint.h>

#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/update_request_task.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class RequestQueueStore;

class MarkAttemptDeferredTask : public UpdateRequestTask {
 public:
  MarkAttemptDeferredTask(RequestQueueStore* store,
                          int64_t request_id,
                          RequestQueueStore::UpdateCallback callback);
  ~MarkAttemptDeferredTask() override;

 protected:
  // UpdateRequestTask implementation:
  void UpdateRequestImpl(UpdateRequestsResult result) override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_DEFERRED_TASK_H_
