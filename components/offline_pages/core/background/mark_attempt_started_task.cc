// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/mark_attempt_started_task.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/offline_pages/core/offline_clock.h"

namespace offline_pages {

MarkAttemptStartedTask::MarkAttemptStartedTask(
    RequestQueueStore* store,
    int64_t request_id,
    RequestQueueStore::UpdateCallback callback)
    : UpdateRequestTask(store, request_id, std::move(callback)) {}

MarkAttemptStartedTask::~MarkAttemptStartedTask() = default;

void MarkAttemptStartedTask::UpdateRequestImpl(
    UpdateRequestsResult read_result) {
  if (!ValidateReadResult(read_result)) {
    CompleteWithResult(std::move(read_result));
    return;
  }

  // It is perfectly fine to reuse the read_result.updated_items collection, as
  // it is owned by this callback and will be destroyed when out of scope.
  read_result.updated_items[0].MarkAttemptStarted(OfflineTimeNow());
  store()->UpdateRequests(
      read_result.updated_items,
      base::BindOnce(&MarkAttemptStartedTask::CompleteWithResult,
                     GetWeakPtr()));
}

}  // namespace offline_pages
