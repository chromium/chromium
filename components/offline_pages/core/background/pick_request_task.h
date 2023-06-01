// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_PICK_REQUEST_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_PICK_REQUEST_TASK_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class OfflinerPolicy;
class PickRequestTask;
class RequestQueueStore;

typedef bool (PickRequestTask::*RequestCompareFunction)(
    const SavePageRequest& left,
    const SavePageRequest& right);

class PickRequestTask : public Task {
 public:
  static const base::TimeDelta kDeferInterval;

  // Callback to report when a request was available.
  typedef base::OnceCallback<void(
      const SavePageRequest& request,
      std::unique_ptr<std::vector<SavePageRequest>> available_requests,
      bool cleanup_needed)>
      RequestPickedCallback;

  // Callback to report when no request was available.
  typedef base::OnceCallback<void(bool non_user_requests,
                                  bool cleanup_needed,
                                  base::Time available_time)>
      RequestNotPickedCallback;

  // Callback to report available total and available queued request counts.
  typedef base::OnceCallback<void(size_t, size_t)> RequestCountCallback;

  PickRequestTask(RequestQueueStore* store,
                  OfflinerPolicy* policy,
                  RequestPickedCallback picked_callback,
                  RequestNotPickedCallback not_picked_callback,
                  DeviceConditions device_conditions,
                  const std::set<int64_t>& disabled_requests,
                  base::circular_deque<int64_t>* prioritized_requests);

  ~PickRequestTask() override;

 private:
  // TaskQueue::Task implementation, starts the async chain
  void Run() override;

  // Step 1. get the requests
  void GetRequests();

  // Step 2. pick a request that we like best from available requests.
  void Choose(bool get_succeeded,
              std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Helper functions.

  // Determine if this request has device conditions appropriate for running it.
  bool RequestConditionsSatisfied(const SavePageRequest& request);

  // Determine if the new request is preferred under current policies.
  bool IsNewRequestBetter(const SavePageRequest& oldRequest,
                          const SavePageRequest& newRequest,
                          RequestCompareFunction comparator);

  // Returns true if the left hand side is better.
  bool RetryCountFirstCompareFunction(const SavePageRequest& left,
                                      const SavePageRequest& right);

  // Returns true if the left hand side is better.
  bool RecencyFirstCompareFunction(const SavePageRequest& left,
                                   const SavePageRequest& right);

  // Compare left and right side, returning 1 if the left side is better
  // (preferred by policy), 0 if the same, and -1 if the right side is better.
  int CompareRetryCount(const SavePageRequest& left,
                        const SavePageRequest& right);

  // Compare left and right side, returning 1 if the left side is better
  // (preferred by policy), 0 if the same, and -1 if the right side is better.
  int CompareCreationTime(const SavePageRequest& left,
                          const SavePageRequest& right);

  // Member variables, all pointers are not owned here.
  raw_ptr<RequestQueueStore> store_;
  raw_ptr<OfflinerPolicy, DanglingUntriaged> policy_;
  RequestPickedCallback picked_callback_;
  RequestNotPickedCallback not_picked_callback_;
  DeviceConditions device_conditions_;
  const raw_ref<const std::set<int64_t>> disabled_requests_;
  // TODO(harringtond): This object is owned by the caller, and mutating it
  // directly here seems dangerous.
  raw_ptr<base::circular_deque<int64_t>> prioritized_requests_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<PickRequestTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_PICK_REQUEST_TASK_H_
