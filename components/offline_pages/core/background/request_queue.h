// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_pages/core/background/cleanup_task_factory.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/pick_request_task.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/task/task_queue.h"

namespace offline_pages {

class CleanupTaskFactory;
class RequestQueueStore;

// Class responsible for managing save page requests.
class RequestQueue : public TaskQueue::Delegate {
 public:
  // Callback used for |GetRequests|.
  typedef base::OnceCallback<
      void(GetRequestsResult, std::vector<std::unique_ptr<SavePageRequest>>)>
      GetRequestsCallback;

  // Callback used for |AddRequest|.
  typedef base::OnceCallback<void(AddRequestResult,
                                  const SavePageRequest& request)>
      AddRequestCallback;

  // Callback used by |ChangeRequestsState|.
  typedef base::OnceCallback<void(UpdateRequestsResult)> UpdateCallback;

  // Callback used by |UdpateRequest|.
  typedef base::OnceCallback<void(UpdateRequestResult)> UpdateRequestCallback;

  struct AddOptions {
    // If non-zero, the request will only be added to the queue if fewer than
    // this many requests are already in the queue. This only considers other
    // requests with the same |client_id.name_space|. If there are too many
    // requests, |AddRequestResult::REQUEST_QUOTA_HIT| will be returned.
    int maximum_in_flight_requests_for_namespace = 0;

    // If true, the request will only be added to the queue if the URL being
    // requested is unique for all active requests from the same namespace.
    bool disallow_duplicate_requests = false;
  };

  explicit RequestQueue(std::unique_ptr<RequestQueueStore> store);

  RequestQueue(const RequestQueue&) = delete;
  RequestQueue& operator=(const RequestQueue&) = delete;

  ~RequestQueue() override;

  // TaskQueue::Delegate
  void OnTaskQueueIsIdle() override;

  // Gets all of the active requests from the store. Calling this method may
  // schedule purging of the request queue.
  void GetRequests(GetRequestsCallback callback);

  // Adds |request| to the request queue. Result is returned through |callback|.
  // In case adding the request violates policy, the result will fail with
  // appropriate result. Callback will also return a copy of a request with all
  // fields set.
  void AddRequest(const SavePageRequest& request,
                  AddOptions options,
                  AddRequestCallback callback);

  // Removes the requests matching the |request_ids|. Result is returned through
  // |callback|.  If a request id cannot be removed, this will still remove the
  // others.
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      UpdateCallback callback);

  // Invokes |remove_predicate| for all requests in the queue, and removes each
  // request where |remove_predicate| returns true. Note: |remove_predicate| is
  // called from a background thread.
  void RemoveRequestsIf(const base::RepeatingCallback<
                            bool(const SavePageRequest&)>& remove_predicate,
                        UpdateCallback done_callback);

  // Changes the state to |new_state| for requests matching the
  // |request_ids|. Results are returned through |callback|.
  void ChangeRequestsState(const std::vector<int64_t>& request_ids,
                           const SavePageRequest::RequestState new_state,
                           UpdateCallback callback);

  // Marks attempt with |request_id| as started. Results are returned through
  // |callback|.
  void MarkAttemptStarted(int64_t request_id, UpdateCallback callback);

  // Marks attempt with |request_id| as aborted. Results are returned through
  // |callback|.
  void MarkAttemptAborted(int64_t request_id, UpdateCallback callback);

  // Marks attempt with |request_id| as deferred. Results are returned through
  // |callback|.
  void MarkAttemptDeferred(int64_t request_id, UpdateCallback callback);

  // Marks attempt with |request_id| as completed. The attempt may have
  // completed with either success or failure (stored in FailState). Results are
  // returned through |callback|.
  void MarkAttemptCompleted(int64_t request_id,
                            FailState fail_state,
                            UpdateCallback callback);

  // Sets the auto fetch notification state on the request with |request_id|.
  void SetAutoFetchNotificationState(
      int64_t request_id,
      SavePageRequest::AutoFetchNotificationState state,
      base::OnceCallback<void(bool updated)> callback);

  // Make a task to pick the next request, and report our choice to the
  // callbacks.
  void PickNextRequest(
      OfflinerPolicy* policy,
      PickRequestTask::RequestPickedCallback picked_callback,
      PickRequestTask::RequestNotPickedCallback not_picked_callback,
      DeviceConditions conditions,
      const std::set<int64_t>& disabled_requests,
      base::circular_deque<int64_t>* prioritized_requests);

  // Reconcile any requests that were active the last time chrome exited.
  void ReconcileRequests(UpdateCallback callback);

  // Cleanup requests that have expired, exceeded the start or completed retry
  // limit.
  void CleanupRequestQueue();

  // Takes ownership of the factory.  We use a setter to allow users of the
  // request queue to not need a CleanupFactory to create it, since we have lots
  // of code using the request queue.  The request coordinator will set a
  // factory before calling CleanupRequestQueue.
  void SetCleanupFactory(std::unique_ptr<CleanupTaskFactory> factory) {
    cleanup_factory_ = std::move(factory);
  }

  RequestQueueStore* GetStoreForTesting() { return store_.get(); }

 private:
  // Store initialization functions.
  void Initialize();
  void InitializeStoreDone(bool success);

  std::unique_ptr<RequestQueueStore> store_;

  // Task queue to serialize store access.
  TaskQueue task_queue_;

  // Builds CleanupTask objects.
  std::unique_ptr<CleanupTaskFactory> cleanup_factory_;

  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestQueue> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_H_
