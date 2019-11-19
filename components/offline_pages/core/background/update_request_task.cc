// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/update_request_task.h"

#include <vector>

#include "base/bind.h"
#include "base/time/time.h"

namespace offline_pages {

UpdateRequestTask::UpdateRequestTask(RequestQueueStore* store,
                                     int64_t request_id,
                                     RequestQueueStore::UpdateCallback callback)
    : store_(store), request_id_(request_id), callback_(std::move(callback)) {}

UpdateRequestTask::~UpdateRequestTask() {}

void UpdateRequestTask::Run() {
  ReadRequest();
}

void UpdateRequestTask::ReadRequest() {
  std::vector<int64_t> request_ids{request_id_};
  store_->GetRequestsByIds(request_ids,
                           base::BindOnce(&UpdateRequestTask::UpdateRequestImpl,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void UpdateRequestTask::CompleteWithResult(UpdateRequestsResult result) {
  std::move(callback_).Run(std::move(result));
  TaskComplete();
}

bool UpdateRequestTask::ValidateReadResult(const UpdateRequestsResult& result) {
  return result.store_state == StoreState::LOADED &&
         result.item_statuses.size() == 1 &&
         result.item_statuses.at(0).first == request_id() &&
         result.item_statuses.at(0).second == ItemActionStatus::SUCCESS &&
         result.updated_items.size() == 1 &&
         result.updated_items.at(0).request_id() == request_id();
}

}  // namespace offline_pages
