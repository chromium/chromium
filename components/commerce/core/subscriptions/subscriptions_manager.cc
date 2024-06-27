// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/subscriptions/subscriptions_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include <queue>
#include <string>

namespace commerce {

namespace {
const int kDefaultTimeoutMs = 10000;
const char kTimeoutParam[] = "subscriptions_request_timeout";
constexpr base::FeatureParam<int> kTimeoutMs{&commerce::kShoppingList,
                                             kTimeoutParam, kDefaultTimeoutMs};
}  // namespace

const char kTrackResultHistogramName[] = "Commerce.Subscriptions.TrackResult";
const char kUntrackResultHistogramName[] =
    "Commerce.Subscriptions.UntrackResult";

SubscriptionsManager::SubscriptionsManager(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    SessionProtoStorage<
        commerce_subscription_db::CommerceSubscriptionContentProto>*
        subscription_proto_db,
    AccountChecker* account_checker)
    : SubscriptionsManager(
          identity_manager,
          std::make_unique<SubscriptionsServerProxy>(
              identity_manager,
              std::move(url_loader_factory)),
          std::make_unique<SubscriptionsStorage>(subscription_proto_db),
          account_checker) {}

SubscriptionsManager::SubscriptionsManager(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SubscriptionsServerProxy> server_proxy,
    std::unique_ptr<SubscriptionsStorage> storage,
    AccountChecker* account_checker)
    : server_proxy_(std::move(server_proxy)),
      storage_(std::move(storage)),
      account_checker_(account_checker),
      observers_(base::ObserverListPolicy::EXISTING_ONLY) {
  SyncSubscriptions();
  scoped_identity_manager_observation_.Observe(identity_manager);
}

SubscriptionsManager::SubscriptionsManager() = default;

SubscriptionsManager::~SubscriptionsManager() = default;

SubscriptionsManager::Request::Request(AsyncOperation operation,
                                       base::OnceCallback<void()> callback)
    : operation(operation), callback(std::move(callback)) {}
SubscriptionsManager::Request::Request(Request&&) = default;
SubscriptionsManager::Request::~Request() = default;

void SubscriptionsManager::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  CHECK(subscriptions->size() > 0);

  SyncIfNeeded();

  pending_requests_.emplace(
      AsyncOperation::kSubscribe,
      base::BindOnce(&SubscriptionsManager::HandleSubscribe,
                     weak_ptr_factory_.GetWeakPtr(), std::move(subscriptions),
                     std::move(callback)));
  CheckAndProcessRequest();
}

void SubscriptionsManager::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  CHECK(subscriptions->size() > 0);

  SyncIfNeeded();

  pending_requests_.emplace(
      AsyncOperation::kUnsubscribe,
      base::BindOnce(&SubscriptionsManager::HandleUnsubscribe,
                     weak_ptr_factory_.GetWeakPtr(), std::move(subscriptions),
                     std::move(callback)));
  CheckAndProcessRequest();
}

void SubscriptionsManager::SyncSubscriptions() {
  pending_requests_.emplace(AsyncOperation::kSync,
                            base::BindOnce(&SubscriptionsManager::HandleSync,
                                           weak_ptr_factory_.GetWeakPtr()));
  CheckAndProcessRequest();
}

void SubscriptionsManager::IsSubscribed(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  SyncIfNeeded();

  pending_requests_.emplace(
      AsyncOperation::kLookupOne,
      base::BindOnce(&SubscriptionsManager::HandleLookup,
                     weak_ptr_factory_.GetWeakPtr(), std::move(subscription),
                     std::move(callback)));
  CheckAndProcessRequest();
}

bool SubscriptionsManager::IsSubscribedFromCache(
    const CommerceSubscription& subscription) {
  return storage_->IsSubscribedFromCache(subscription);
}

void SubscriptionsManager::GetAllSubscriptions(
    SubscriptionType type,
    base::OnceCallback<void(std::vector<CommerceSubscription>)> callback) {
  SyncIfNeeded();

  pending_requests_.emplace(AsyncOperation::kGetAll,
                            base::BindOnce(&SubscriptionsManager::HandleGetAll,
                                           weak_ptr_factory_.GetWeakPtr(), type,
                                           std::move(callback)));
  CheckAndProcessRequest();
}

void SubscriptionsManager::CheckTimestampOnBookmarkChange(
    int64_t bookmark_subscription_change_time) {
  pending_requests_.emplace(
      AsyncOperation::kCheckOnBookmarkChange,
      base::BindOnce(
          &SubscriptionsManager::HandleCheckTimestampOnBookmarkChange,
          weak_ptr_factory_.GetWeakPtr(), bookmark_subscription_change_time));
  CheckAndProcessRequest();
}

void SubscriptionsManager::CheckAndProcessRequest() {
  if (HasRequestRunning() || pending_requests_.empty())
    return;

  // If there is no request running, we can start processing next request in the
  // queue.
  has_request_running_ = true;
  last_request_started_time_ = base::Time::Now();
  Request request = std::move(pending_requests_.front());
  pending_requests_.pop();
  last_request_operation_ = request.operation;
  std::move(request.callback).Run();
}

void SubscriptionsManager::SyncIfNeeded() {
  if (!last_sync_succeeded_ && !HasRequestRunning()) {
    SyncSubscriptions();
  }
}

void SubscriptionsManager::OnRequestCompletion() {
  has_request_running_ = false;
  CheckAndProcessRequest();
}

void SubscriptionsManager::UpdateSyncStates(bool sync_succeeded) {
  last_sync_succeeded_ = sync_succeeded;
  if (sync_succeeded) {
    // Always use the Windows epoch to keep consistency with the timestamp in
    // bookmark.
    last_sync_time_ =
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  }
}

void SubscriptionsManager::HandleSync() {
  if (account_checker_ && account_checker_->IsSignedIn() &&
      account_checker_->IsAnonymizedUrlDataCollectionEnabled()) {
    GetRemoteSubscriptionsAndUpdateStorage(
        SubscriptionType::kPriceTrack,
        base::BindOnce(&SubscriptionsManager::OnSyncStatusFetched,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SubscriptionsManager::OnSyncStatusFetched(
    SubscriptionsRequestStatus result) {
  UpdateSyncStates(result == SubscriptionsRequestStatus::kSuccess);
  OnRequestCompletion();
}

void SubscriptionsManager::HandleSubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  SubscriptionType type = (*subscriptions)[0].type;
  // Make a copy of subscriptions to notify observers later.
  std::vector<CommerceSubscription> notified_subscriptions = *subscriptions;

  SubscriptionsRequestCallback wrapped_callback =
      base::BindOnce(&SubscriptionsManager::OnSubscribeStatusFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(notified_subscriptions), std::move(callback));

  if (!last_sync_succeeded_) {
    std::move(wrapped_callback)
        .Run(SubscriptionsRequestStatus::kLastSyncFailed);
    return;
  }
  storage_->GetUniqueNonExistingSubscriptions(
      std::move(subscriptions),
      base::BindOnce(
          &SubscriptionsManager::OnIncomingSubscriptionsFilteredForSubscribe,
          weak_ptr_factory_.GetWeakPtr(), type, std::move(wrapped_callback)));
}

void SubscriptionsManager::OnSubscribeStatusFetched(
    std::vector<CommerceSubscription> notified_subscriptions,
    base::OnceCallback<void(bool)> callback,
    SubscriptionsRequestStatus result) {
  base::UmaHistogramEnumeration(kTrackResultHistogramName, result);
  bool succeeded = result == SubscriptionsRequestStatus::kSuccess ||
                   result == SubscriptionsRequestStatus::kNoOp;
  OnSubscribe(notified_subscriptions, succeeded);
  std::move(callback).Run(succeeded);
  // We sync local cache with server only when the product is successfully added
  // on server. The sync states should be updated after notifying all observers
  // and running the callback so |last_sync_time_| is larger than external
  // timestamp (e.g. in bookmarks).
  if (result == SubscriptionsRequestStatus::kSuccess) {
    UpdateSyncStates(true);
  }
  OnRequestCompletion();
}

void SubscriptionsManager::OnIncomingSubscriptionsFilteredForSubscribe(
    SubscriptionType type,
    SubscriptionsRequestCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> unique_subscriptions) {
  if (unique_subscriptions->size() == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SubscriptionsRequestStatus::kNoOp));
    return;
  }
  server_proxy_->Create(
      std::move(unique_subscriptions),
      base::BindOnce(&SubscriptionsManager::HandleManageSubscriptionsResponse,
                     weak_ptr_factory_.GetWeakPtr(), type,
                     std::move(callback)));
}

void SubscriptionsManager::HandleUnsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  SubscriptionType type = (*subscriptions)[0].type;
  // Make a copy of subscriptions to notify observers later.
  std::vector<CommerceSubscription> notified_subscriptions = *subscriptions;

  SubscriptionsRequestCallback wrapped_callback =
      base::BindOnce(&SubscriptionsManager::OnUnsubscribeStatusFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(notified_subscriptions), std::move(callback));

  if (!last_sync_succeeded_) {
    std::move(wrapped_callback)
        .Run(SubscriptionsRequestStatus::kLastSyncFailed);
    return;
  }
  storage_->GetUniqueExistingSubscriptions(
      std::move(subscriptions),
      base::BindOnce(
          &SubscriptionsManager::OnIncomingSubscriptionsFilteredForUnsubscribe,
          weak_ptr_factory_.GetWeakPtr(), type, std::move(wrapped_callback)));
}

void SubscriptionsManager::OnUnsubscribeStatusFetched(
    std::vector<CommerceSubscription> notified_subscriptions,
    base::OnceCallback<void(bool)> callback,
    SubscriptionsRequestStatus result) {
  base::UmaHistogramEnumeration(kUntrackResultHistogramName, result);
  bool succeeded = result == SubscriptionsRequestStatus::kSuccess ||
                   result == SubscriptionsRequestStatus::kNoOp;
  OnUnsubscribe(notified_subscriptions, succeeded);
  std::move(callback).Run(succeeded);
  // We sync local cache with server only when the product is successfully
  // removed on server. The sync states should be updated after notifying all
  // observers and running the callback so |last_sync_time_| is larger than
  // external timestamp (e.g. in bookmarks).
  if (result == SubscriptionsRequestStatus::kSuccess) {
    UpdateSyncStates(true);
  }
  OnRequestCompletion();
}

void SubscriptionsManager::OnIncomingSubscriptionsFilteredForUnsubscribe(
    SubscriptionType type,
    SubscriptionsRequestCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> unique_subscriptions) {
  if (unique_subscriptions->size() == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SubscriptionsRequestStatus::kNoOp));
    return;
  }
  server_proxy_->Delete(
      std::move(unique_subscriptions),
      base::BindOnce(&SubscriptionsManager::HandleManageSubscriptionsResponse,
                     weak_ptr_factory_.GetWeakPtr(), type,
                     std::move(callback)));
}

void SubscriptionsManager::GetRemoteSubscriptionsAndUpdateStorage(
    SubscriptionType type,
    SubscriptionsRequestCallback callback) {
  server_proxy_->Get(
      type, base::BindOnce(
                &SubscriptionsManager::HandleGetSubscriptionsResponse,
                weak_ptr_factory_.GetWeakPtr(), type, std::move(callback)));
}

void SubscriptionsManager::HandleGetSubscriptionsResponse(
    SubscriptionType type,
    SubscriptionsRequestCallback callback,
    SubscriptionsRequestStatus status,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions) {
  if (status != SubscriptionsRequestStatus::kSuccess) {
    std::move(callback).Run(status);
  } else {
    storage_->UpdateStorage(type, std::move(callback),
                            std::move(remote_subscriptions));
  }
}

void SubscriptionsManager::HandleManageSubscriptionsResponse(
    SubscriptionType type,
    SubscriptionsRequestCallback callback,
    SubscriptionsRequestStatus status,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions) {
  if (status != SubscriptionsRequestStatus::kSuccess) {
    VLOG(1) << "Fail to create or delete subscriptions on server";
    std::move(callback).Run(status);
  } else {
    storage_->UpdateStorage(type, std::move(callback),
                            std::move(remote_subscriptions));
  }
}

void SubscriptionsManager::HandleLookup(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  storage_->IsSubscribed(
      std::move(subscription),
      base::BindOnce(&SubscriptionsManager::OnLookupResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsManager::OnLookupResult(
    base::OnceCallback<void(bool)> callback,
    bool is_subscribed) {
  std::move(callback).Run(is_subscribed);
  OnRequestCompletion();
}

void SubscriptionsManager::HandleGetAll(
    SubscriptionType type,
    base::OnceCallback<void(std::vector<CommerceSubscription>)> callback) {
  storage_->LoadAllSubscriptionsForType(
      type,
      base::BindOnce(&SubscriptionsManager::OnGetAllResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsManager::OnGetAllResult(
    base::OnceCallback<void(std::vector<CommerceSubscription>)> callback,
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
  std::move(callback).Run(std::move(*subscriptions));
  OnRequestCompletion();
}

void SubscriptionsManager::HandleCheckTimestampOnBookmarkChange(
    int64_t bookmark_subscription_change_time) {
  // Do nothing if current local cache is newer than the bookmark change.
  if (bookmark_subscription_change_time <= last_sync_time_) {
    OnRequestCompletion();
    return;
  }

  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          &SubscriptionsManager::HandleGetSubscriptionsResponseOnBookmarkChange,
          weak_ptr_factory_.GetWeakPtr()));
}

void SubscriptionsManager::HandleGetSubscriptionsResponseOnBookmarkChange(
    SubscriptionsRequestStatus status,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions) {
  if (status != SubscriptionsRequestStatus::kSuccess) {
    UpdateSyncStates(false);
    OnRequestCompletion();
    return;
  }

  storage_->UpdateStorageAndNotifyModifiedSubscriptions(
      SubscriptionType::kPriceTrack,
      base::BindOnce(&SubscriptionsManager::OnStorageUpdatedOnBookmarkChange,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(remote_subscriptions));
}

void SubscriptionsManager::OnStorageUpdatedOnBookmarkChange(
    SubscriptionsRequestStatus status,
    std::vector<CommerceSubscription> added_subs,
    std::vector<CommerceSubscription> removed_subs) {
  if (status == SubscriptionsRequestStatus::kSuccess) {
    if (added_subs.size() > 0) {
      OnSubscribe(added_subs, true);
    }
    if (removed_subs.size() > 0) {
      OnUnsubscribe(removed_subs, true);
    }
  }
  UpdateSyncStates(status == SubscriptionsRequestStatus::kSuccess);
  OnRequestCompletion();
}

void SubscriptionsManager::OnSubscribe(
    const std::vector<CommerceSubscription>& subscriptions,
    bool succeeded) {
  for (SubscriptionsObserver& observer : observers_) {
    for (auto& sub : subscriptions) {
      observer.OnSubscribe(sub, succeeded);
    }
  }
}

void SubscriptionsManager::OnUnsubscribe(
    const std::vector<CommerceSubscription>& subscriptions,
    bool succeeded) {
  for (SubscriptionsObserver& observer : observers_) {
    for (auto& sub : subscriptions) {
      observer.OnUnsubscribe(sub, succeeded);
    }
  }
}

void SubscriptionsManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  storage_->DeleteAll();
  SyncSubscriptions();
}

bool SubscriptionsManager::HasRequestRunning() {
  // Reset has_request_running_ to false if the last request is stuck somewhere.
  // TODO(crbug.com/40241090): We should still be able to get the callback when
  // the request times out. Also we should make the callback cancelable itself
  // rather than having to wait for the next request coming.
  if (has_request_running_ &&
      (base::Time::Now() - last_request_started_time_).InMilliseconds() >
          kTimeoutMs.Get()) {
    has_request_running_ = false;
    if (last_request_operation_ == AsyncOperation::kSubscribe) {
      base::UmaHistogramEnumeration(kTrackResultHistogramName,
                                    SubscriptionsRequestStatus::kLost);
    } else if (last_request_operation_ == AsyncOperation::kUnsubscribe) {
      base::UmaHistogramEnumeration(kUntrackResultHistogramName,
                                    SubscriptionsRequestStatus::kLost);
    }
  }
  return has_request_running_;
}

void SubscriptionsManager::AddObserver(SubscriptionsObserver* observer) {
  observers_.AddObserver(observer);
}

void SubscriptionsManager::RemoveObserver(SubscriptionsObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool SubscriptionsManager::GetLastSyncSucceededForTesting() {
  return last_sync_succeeded_;
}

int64_t SubscriptionsManager::GetLastSyncTimeForTesting() {
  return last_sync_time_;
}

void SubscriptionsManager::SetHasRequestRunningForTesting(
    bool has_request_running) {
  has_request_running_ = has_request_running;
}

bool SubscriptionsManager::HasPendingRequestsForTesting() {
  return !pending_requests_.empty();
}

void SubscriptionsManager::SetLastRequestStartedTimeForTesting(
    base::Time time) {
  last_request_started_time_ = time;
}

}  // namespace commerce
