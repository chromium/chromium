// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_ABORTED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_ABORTED_TASK_H_

#include <stdint.h>

#include "components/offline_pages/core/background/update_request_task.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class RequestQueueStore;

class MarkAttemptAbortedTask : public UpdateRequestTask {
 public:
  MarkAttemptAbortedTask(RequestQueueStore* store,
                         int64_t request_id,
                         RequestQueueStore::UpdateCallback callback);
  ~MarkAttemptAbortedTask() override;

 protected:
  // UpdateRequestTask implementation:
  void UpdateRequestImpl(UpdateRequestsResult result) override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_MARK_ATTEMPT_ABORTED_TASK_H_
