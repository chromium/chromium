// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/remove_requests_task.h"

#include "base/bind.h"

namespace offline_pages {

RemoveRequestsTask::RemoveRequestsTask(
    RequestQueueStore* store,
    const std::vector<int64_t>& request_ids,
    RequestQueueStore::UpdateCallback callback)
    : store_(store),
      request_ids_(request_ids),
      callback_(std::move(callback)) {}

RemoveRequestsTask::~RemoveRequestsTask() {}

void RemoveRequestsTask::Run() {
  RemoveRequests();
}

void RemoveRequestsTask::RemoveRequests() {
  if (request_ids_.empty()) {
    CompleteEarly(ItemActionStatus::NOT_FOUND);
    return;
  }

  store_->RemoveRequests(request_ids_,
                         base::BindOnce(&RemoveRequestsTask::CompleteWithResult,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void RemoveRequestsTask::CompleteEarly(ItemActionStatus status) {
  UpdateRequestsResult result(store_->state());
  for (int64_t request_id : request_ids_)
    result.item_statuses.emplace_back(request_id, status);
  CompleteWithResult(std::move(result));
}

void RemoveRequestsTask::CompleteWithResult(UpdateRequestsResult result) {
  std::move(callback_).Run(std::move(result));
  TaskComplete();
}

}  // namespace offline_pages
