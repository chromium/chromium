// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/get_requests_task.h"

#include <vector>

#include "base/functional/bind.h"

namespace offline_pages {

GetRequestsTask::GetRequestsTask(
    RequestQueueStore* store,
    RequestQueueStore::GetRequestsCallback callback)
    : store_(store), callback_(std::move(callback)) {}

GetRequestsTask::~GetRequestsTask() = default;

void GetRequestsTask::Run() {
  ReadRequest();
}

void GetRequestsTask::ReadRequest() {
  store_->GetRequests(base::BindOnce(&GetRequestsTask::CompleteWithResult,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void GetRequestsTask::CompleteWithResult(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  std::move(callback_).Run(success, std::move(requests));
  TaskComplete();
}

}  // namespace offline_pages
