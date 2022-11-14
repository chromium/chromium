// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
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

const char kTrackResultHistogramName[] = "Commerce.Subscriptions.TrackResult";
const char kUntrackResultHistogramName[] =
    "Commerce.Subscriptions.UntrackResult";
}  // namespace

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
      weak_ptr_factory_(this) {
// Avoid duplicate server calls on android. Remove this after we integrate
// android implementation to shopping service.
#if !BUILDFLAG(IS_ANDROID)
  SyncSubscriptions();
  scoped_identity_manager_observation_.Observe(identity_manager);
#endif  // !BUILDFLAG(IS_ANDROID)
}

SubscriptionsManager::~SubscriptionsManager() = default;

SubscriptionsManager::Request::Request(SubscriptionType type,
                                       AsyncOperation operation,
                                       SubscriptionsRequestCallback callback)
    : type(type), operation(operation), callback(std::move(callback)) {
  CHECK(operation == AsyncOperation::kSync);
}
SubscriptionsManager::Request::Request(
    SubscriptionType type,
    AsyncOperation operation,
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    SubscriptionsRequestCallback callback)
    : type(type),
      operation(operation),
      subscriptions(std::move(subscriptions)),
      callback(std::move(callback)) {
  CHECK(operation == AsyncOperation::kSubscribe ||
        operation == AsyncOperation::kUnsubscribe);
}
SubscriptionsManager::Request::Request(Request&&) = default;
SubscriptionsManager::Request::~Request() = default;

void SubscriptionsManager::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  // If there is a coming subscribe request but the last sync with the server
  // failed, we should re-try the sync, or this request will fail directly.
  if (!last_sync_succeeded_ && !HasRequestRunning()) {
    SyncSubscriptions();
  }
  SubscriptionType type = (*subscriptions)[0].type;
  pending_requests_.emplace(
      type, AsyncOperation::kSubscribe, std::move(subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager,
             base::OnceCallback<void(bool)> callback,
             SubscriptionsRequestStatus result) {
            base::UmaHistogramEnumeration(kTrackResultHistogramName, result);
            std::move(callback).Run(result ==
                                    SubscriptionsRequestStatus::kSuccess);
            manager->OnRequestCompletion();
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  CheckAndProcessRequest();
}

void SubscriptionsManager::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  // If there is a coming unsubscribe request but the last sync with the server
  // failed, we should re-try the sync, or this request will fail directly.
  if (!last_sync_succeeded_ && !HasRequestRunning()) {
    SyncSubscriptions();
  }
  SubscriptionType type = (*subscriptions)[0].type;
  pending_requests_.emplace(
      type, AsyncOperation::kUnsubscribe, std::move(subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager,
             base::OnceCallback<void(bool)> callback,
             SubscriptionsRequestStatus result) {
            base::UmaHistogramEnumeration(kUntrackResultHistogramName, result);
            std::move(callback).Run(result ==
                                    SubscriptionsRequestStatus::kSuccess);
            manager->OnRequestCompletion();
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  CheckAndProcessRequest();
}

void SubscriptionsManager::SyncSubscriptions() {
  last_sync_succeeded_ = false;
  storage_->DeleteAll();
  if (base::FeatureList::IsEnabled(commerce::kShoppingList) &&
      account_checker_ && account_checker_->IsSignedIn() &&
      account_checker_->IsAnonymizedUrlDataCollectionEnabled()) {
    pending_requests_.emplace(
        SubscriptionType::kPriceTrack, AsyncOperation::kSync,
        base::BindOnce(
            [](base::WeakPtr<SubscriptionsManager> manager,
               SubscriptionsRequestStatus result) {
              manager->last_sync_succeeded_ =
                  result == SubscriptionsRequestStatus::kSuccess;
              manager->OnRequestCompletion();
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
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
  CHECK(request.type != SubscriptionType::kTypeUnspecified);
  last_request_operation_ = request.operation;

  switch (request.operation) {
    case AsyncOperation::kSync:
      ProcessSyncRequest(std::move(request));
      break;
    case AsyncOperation::kSubscribe:
      ProcessSubscribeRequest(std::move(request));
      break;
    case AsyncOperation::kUnsubscribe:
      ProcessUnsubscribeRequest(std::move(request));
      break;
  }
}

void SubscriptionsManager::OnRequestCompletion() {
  has_request_running_ = false;
  CheckAndProcessRequest();
}

void SubscriptionsManager::ProcessSyncRequest(Request request) {
  GetRemoteSubscriptionsAndUpdateStorage(request.type,
                                         std::move(request.callback));
}

void SubscriptionsManager::ProcessSubscribeRequest(Request request) {
  if (!last_sync_succeeded_) {
    std::move(request.callback)
        .Run(SubscriptionsRequestStatus::kLastSyncFailed);
    return;
  }
  storage_->GetUniqueNonExistingSubscriptions(
      std::move(request.subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager, SubscriptionType type,
             SubscriptionsRequestCallback callback,
             std::unique_ptr<std::vector<CommerceSubscription>>
                 unique_subscriptions) {
            if (unique_subscriptions->size() == 0) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 SubscriptionsRequestStatus::kSuccess));
              return;
            }
            manager->server_proxy_->Create(
                std::move(unique_subscriptions),
                base::BindOnce(
                    &SubscriptionsManager::HandleManageSubscriptionsResponse,
                    manager, type, std::move(callback)));
          },
          weak_ptr_factory_.GetWeakPtr(), request.type,
          std::move(request.callback)));
}

void SubscriptionsManager::ProcessUnsubscribeRequest(Request request) {
  if (!last_sync_succeeded_) {
    std::move(request.callback)
        .Run(SubscriptionsRequestStatus::kLastSyncFailed);
    return;
  }
  storage_->GetUniqueExistingSubscriptions(
      std::move(request.subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager, SubscriptionType type,
             SubscriptionsRequestCallback callback,
             std::unique_ptr<std::vector<CommerceSubscription>>
                 unique_subscriptions) {
            if (unique_subscriptions->size() == 0) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 SubscriptionsRequestStatus::kSuccess));
              return;
            }
            manager->server_proxy_->Delete(
                std::move(unique_subscriptions),
                base::BindOnce(
                    &SubscriptionsManager::HandleManageSubscriptionsResponse,
                    manager, type, std::move(callback)));
          },
          weak_ptr_factory_.GetWeakPtr(), request.type,
          std::move(request.callback)));
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
    SubscriptionsRequestStatus status) {
  if (status != SubscriptionsRequestStatus::kSuccess) {
    VLOG(1) << "Fail to create or delete subscriptions on server";
    std::move(callback).Run(status);
  } else {
    GetRemoteSubscriptionsAndUpdateStorage(type, std::move(callback));
  }
}

void SubscriptionsManager::VerifyIfSubscriptionExists(
    CommerceSubscription subscription,
    bool should_exist) {
  storage_->IsSubscribed(
      std::move(subscription),
      base::BindOnce(
          &SubscriptionsManager::HandleCheckLocalSubscriptionResponse,
          weak_ptr_factory_.GetWeakPtr(), should_exist));
}

void SubscriptionsManager::IsSubscribed(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  storage_->IsSubscribed(std::move(subscription), std::move(callback));
}

void SubscriptionsManager::HandleCheckLocalSubscriptionResponse(
    bool should_exist,
    bool is_subscribed) {
  // Don't sync if there is already a request running to avoid redundant server
  // calls.
  if (should_exist != is_subscribed && !HasRequestRunning()) {
    SyncSubscriptions();
  }
}

void SubscriptionsManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  SyncSubscriptions();
}

bool SubscriptionsManager::HasRequestRunning() {
  // Reset has_request_running_ to false if the last request is stuck somewhere.
  // TODO(crbug.com/1370703): We should still be able to get the callback when
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

bool SubscriptionsManager::GetLastSyncSucceededForTesting() {
  return last_sync_succeeded_;
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
