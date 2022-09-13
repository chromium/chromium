// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/offliner_policy_utils.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class OfflinerPolicy;
class RequestCoordinatorEventLogger;
class RequestNotifier;
class RequestQueueStore;

class CleanupTask : public Task {
 public:
  CleanupTask(RequestQueueStore* store,
              OfflinerPolicy* policy,
              RequestNotifier* notifier,
              RequestCoordinatorEventLogger* logger);
  ~CleanupTask() override;

 private:
  // TaskQueue::Task implementation, starts the async chain
  void Run() override;
  // Step 1. get results from the store
  void GetRequests();

  // Step 2. Prune stale requests
  void Prune(bool success,
             std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Step 3. Send delete notifications for the expired requests.
  void OnRequestsExpired(UpdateRequestsResult result);

  // Build a list of IDs whose request has expired.
  void PopulateExpiredRequestIdsAndReasons(
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Member variables, all pointers are not owned here.
  raw_ptr<RequestQueueStore> store_;
  raw_ptr<OfflinerPolicy> policy_;
  raw_ptr<RequestNotifier> notifier_;
  raw_ptr<RequestCoordinatorEventLogger> event_logger_;

  // Holds a map of expired request IDs and respective expiration reasons.
  std::map<int64_t, OfflinerPolicyUtils::RequestExpirationStatus>
      expired_request_ids_and_reasons_;

  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<CleanupTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_H_
