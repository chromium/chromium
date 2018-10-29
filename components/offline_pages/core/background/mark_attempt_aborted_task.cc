// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/mark_attempt_aborted_task.h"

#include "base/bind.h"

namespace offline_pages {

MarkAttemptAbortedTask::MarkAttemptAbortedTask(
    RequestQueueStore* store,
    int64_t request_id,
    RequestQueueStore::UpdateCallback callback)
    : UpdateRequestTask(store, request_id, std::move(callback)) {}

MarkAttemptAbortedTask::~MarkAttemptAbortedTask() {}

void MarkAttemptAbortedTask::UpdateRequestImpl(
    UpdateRequestsResult read_result) {
  if (!ValidateReadResult(read_result)) {
    CompleteWithResult(std::move(read_result));
    return;
  }

  // It is perfectly fine to reuse the read_result.updated_items collection, as
  // it is owned by this callback and will be destroyed when out of scope.
  read_result.updated_items[0].MarkAttemptAborted();
  store()->UpdateRequests(
      read_result.updated_items,
      base::BindOnce(&MarkAttemptAbortedTask::CompleteWithResult,
                     GetWeakPtr()));
}

}  // namespace offline_pages
