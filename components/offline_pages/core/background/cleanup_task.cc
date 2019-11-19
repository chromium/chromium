// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/cleanup_task.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/offliner_policy_utils.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {
namespace {
RequestNotifier::BackgroundSavePageResult ToBackgroundSavePageResult(
    OfflinerPolicyUtils::RequestExpirationStatus expiration_status) {
  switch (expiration_status) {
    case OfflinerPolicyUtils::RequestExpirationStatus::EXPIRED:
      return RequestNotifier::BackgroundSavePageResult::EXPIRED;
    case OfflinerPolicyUtils::RequestExpirationStatus::START_COUNT_EXCEEDED:
      return RequestNotifier::BackgroundSavePageResult::START_COUNT_EXCEEDED;
    case OfflinerPolicyUtils::RequestExpirationStatus::
        COMPLETION_COUNT_EXCEEDED:
      return RequestNotifier::BackgroundSavePageResult::RETRY_COUNT_EXCEEDED;
    case OfflinerPolicyUtils::RequestExpirationStatus::VALID:
    default:
      NOTREACHED();
      return RequestNotifier::BackgroundSavePageResult::EXPIRED;
  }
}
}  // namespace

CleanupTask::CleanupTask(RequestQueueStore* store,
                         OfflinerPolicy* policy,
                         RequestNotifier* notifier,
                         RequestCoordinatorEventLogger* event_logger)
    : store_(store),
      policy_(policy),
      notifier_(notifier),
      event_logger_(event_logger) {}

CleanupTask::~CleanupTask() {}

void CleanupTask::Run() {
  GetRequests();
}

void CleanupTask::GetRequests() {
  // Get all the requests from the queue, we will classify them in the callback.
  store_->GetRequests(
      base::BindOnce(&CleanupTask::Prune, weak_ptr_factory_.GetWeakPtr()));
}

void CleanupTask::Prune(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If there is nothing to do, return right away.
  if (requests.empty()) {
    TaskComplete();
    return;
  }

  PopulateExpiredRequestIdsAndReasons(std::move(requests));

  // If there are no expired requests processing is done.
  if (expired_request_ids_and_reasons_.size() == 0) {
    TaskComplete();
    return;
  }

  std::vector<int64_t> expired_request_ids;
  for (auto const& id_reason_pair : expired_request_ids_and_reasons_)
    expired_request_ids.push_back(id_reason_pair.first);

  store_->RemoveRequests(expired_request_ids,
                         base::BindOnce(&CleanupTask::OnRequestsExpired,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CleanupTask::OnRequestsExpired(UpdateRequestsResult result) {
  for (const auto& request : result.updated_items) {
    // Ensure we have an expiration reason for this request.
    auto iter = expired_request_ids_and_reasons_.find(request.request_id());
    if (iter == expired_request_ids_and_reasons_.end()) {
      NOTREACHED() << "Expired request not found in deleted results.";
      continue;
    }

    // Establish save page result based on the expiration reason.
    RequestNotifier::BackgroundSavePageResult save_page_result(
        ToBackgroundSavePageResult(iter->second));
    event_logger_->RecordDroppedSavePageRequest(
        request.client_id().name_space, save_page_result, request.request_id());
    notifier_->NotifyCompleted(request, save_page_result);
  }

  // The task is now done, return control to the task queue.
  TaskComplete();
}

void CleanupTask::PopulateExpiredRequestIdsAndReasons(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  for (auto& request : requests) {
    // Check for requests past their expiration time or with too many tries.  If
    // it is not still valid, push the request and the reason onto the deletion
    // list.
    OfflinerPolicyUtils::RequestExpirationStatus status =
        OfflinerPolicyUtils::CheckRequestExpirationStatus(request.get(),
                                                          policy_);

    // If we are not working on this request in an offliner, and it is not
    // valid, put it on a list for removal.  We make the exception for current
    // requests because the request might expire after being chosen and before
    // we call cleanup, and we shouldn't delete the request while offlining it.
    if (status != OfflinerPolicyUtils::RequestExpirationStatus::VALID &&
        request->request_state() != SavePageRequest::RequestState::OFFLINING) {
      expired_request_ids_and_reasons_.emplace(request->request_id(), status);
    }
  }
}

}  // namespace offline_pages
