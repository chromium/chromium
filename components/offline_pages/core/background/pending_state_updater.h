// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_PENDING_STATE_UPDATER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_PENDING_STATE_UPDATER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {

class RequestCoordinator;

// Class for updating the PendingState of requests.
class PendingStateUpdater {
 public:
  PendingStateUpdater(RequestCoordinator* request_coordinator);
  ~PendingStateUpdater();

  // Set PendingState for available requests when network is lost and notify
  // RequestCoordinator::Observers.
  void UpdateRequestsOnLossOfNetwork();

  // Set PendingState for the available requests not picked for offlining and
  // notify RequestCoordinator::Observers.
  void UpdateRequestsOnRequestPicked(
      const int64_t picked_request_id,
      std::unique_ptr<std::vector<SavePageRequest>> available_requests);

  // Set PendingState for a request.
  void SetPendingState(SavePageRequest& request);

 private:
  // Notify RequestCoordinator::Observers that the PendingState changed for
  // multiple requests. Callback for RequestCoordinator::GetAllRequests.
  void NotifyChangedPendingStates(
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Unowned pointer.
  RequestCoordinator* request_coordinator_;

  // Used to determine if available requests need to be updated after a request
  // is picked for offlining. True if requests are currently pending another
  // download to complete. False otherwise.
  bool requests_pending_another_download_;

  base::WeakPtrFactory<PendingStateUpdater> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_PENDING_STATE_UPDATER_H_
