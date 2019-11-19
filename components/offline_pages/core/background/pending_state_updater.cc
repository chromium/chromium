// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/pending_state_updater.h"

#include "base/bind.h"
#include "components/offline_items_collection/core/pending_state.h"
#include "components/offline_pages/core/background/request_coordinator.h"

namespace offline_pages {

PendingStateUpdater::PendingStateUpdater(
    RequestCoordinator* request_coordinator)
    : request_coordinator_(request_coordinator),
      requests_pending_another_download_(false) {}

PendingStateUpdater::~PendingStateUpdater() {}

void PendingStateUpdater::UpdateRequestsOnLossOfNetwork() {
  requests_pending_another_download_ = false;
  request_coordinator_->GetAllRequests(
      base::BindOnce(&PendingStateUpdater::NotifyChangedPendingStates,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingStateUpdater::UpdateRequestsOnRequestPicked(
    const int64_t picked_request_id,
    std::unique_ptr<std::vector<SavePageRequest>> available_requests) {
  // Requests do not need to be updated.
  if (requests_pending_another_download_)
    return;

  // All available requests expect for the picked request changed to waiting
  // for another download to complete.
  for (auto& request : *available_requests) {
    if (request.request_id() != picked_request_id) {
      request.set_pending_state(PendingState::PENDING_ANOTHER_DOWNLOAD);
      request_coordinator_->NotifyChanged(request);
    }
  }
  requests_pending_another_download_ = true;
}

void PendingStateUpdater::SetPendingState(SavePageRequest& request) {
  if (request.request_state() == SavePageRequest::RequestState::AVAILABLE) {
    if (request_coordinator_->state() ==
        RequestCoordinator::RequestCoordinatorState::OFFLINING) {
      request.set_pending_state(PendingState::PENDING_ANOTHER_DOWNLOAD);
    } else {
      requests_pending_another_download_ = false;
      request.set_pending_state(PendingState::PENDING_NETWORK);
    }
  }
}

void PendingStateUpdater::NotifyChangedPendingStates(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  for (const auto& request : requests) {
    request_coordinator_->NotifyChanged(*request);
  }
}

}  // namespace offline_pages
