// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/reconcile_task.h"

#include "base/bind.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {

ReconcileTask::ReconcileTask(RequestQueueStore* store,
                             RequestQueueStore::UpdateCallback callback)
    : store_(store), callback_(std::move(callback)) {}

ReconcileTask::~ReconcileTask() {}

void ReconcileTask::Run() {
  GetRequests();
}

void ReconcileTask::GetRequests() {
  // Get all the requests from the queue, we will reconcile them in the
  // callback.
  store_->GetRequests(base::BindOnce(&ReconcileTask::Reconcile,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ReconcileTask::Reconcile(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If there is nothing to do, return right away, no need to call the callback,
  // since the state of the notifications did not change.
  if (requests.empty()) {
    TaskComplete();
    return;
  }

  // Check for tasks in the OFFLINING state, and change the state back to
  // AVAILABLE.
  std::vector<SavePageRequest> items_to_update;
  for (auto& request : requests) {
    if (request->request_state() == SavePageRequest::RequestState::OFFLINING) {
      request->set_request_state(SavePageRequest::RequestState::AVAILABLE);
      items_to_update.push_back(*request);
    }
  }

  // If there is no work (most common case), just return, no need for a
  // callback.
  if (items_to_update.empty()) {
    TaskComplete();
    return;
  }

  store_->UpdateRequests(items_to_update,
                         base::BindOnce(&ReconcileTask::UpdateCompleted,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ReconcileTask::UpdateCompleted(UpdateRequestsResult update_result) {
  // Send a notification to the UI that these items have updated.
  std::move(callback_).Run(std::move(update_result));
  TaskComplete();
}

}  // namespace offline_pages
