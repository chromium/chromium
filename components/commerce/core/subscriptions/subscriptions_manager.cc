// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"

#include <queue>
#include <string>

namespace commerce {

SubscriptionsManager::SubscriptionsManager() : weak_ptr_factory_(this) {
  InitSubscriptions();
}
SubscriptionsManager::~SubscriptionsManager() = default;

SubscriptionsManager::Request::Request(SubscriptionType type,
                                       AsyncOperation operation,
                                       base::OnceCallback<void(bool)> callback)
    : type(type), operation(operation), callback(std::move(callback)) {
  CHECK(operation == AsyncOperation::kInit);
}
SubscriptionsManager::Request::Request(
    SubscriptionType type,
    AsyncOperation operation,
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback)
    : type(type),
      operation(operation),
      subscriptions(std::move(subscriptions)),
      callback(std::move(callback)) {
  CHECK(operation == AsyncOperation::kSubscribe ||
        operation == AsyncOperation::kUnsubscribe);
}
SubscriptionsManager::Request::~Request() = default;

void SubscriptionsManager::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  SubscriptionType type = (*subscriptions)[0].type;
  pending_requests_.push(std::make_unique<Request>(
      type, AsyncOperation::kSubscribe, std::move(subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager,
             base::OnceCallback<void(bool)> callback, bool result) {
            std::move(callback).Run(result);
            manager->OnRequestCompletion();
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
  CheckAndProcessRequest();
}

void SubscriptionsManager::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  SubscriptionType type = (*subscriptions)[0].type;
  pending_requests_.push(std::make_unique<Request>(
      type, AsyncOperation::kUnsubscribe, std::move(subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager,
             base::OnceCallback<void(bool)> callback, bool result) {
            std::move(callback).Run(result);
            manager->OnRequestCompletion();
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
  CheckAndProcessRequest();
}

void SubscriptionsManager::InitSubscriptions() {
  init_succeeded_ = false;
  if (base::FeatureList::IsEnabled(commerce::kShoppingList)) {
    pending_requests_.push(std::make_unique<Request>(
        SubscriptionType::kPriceTrack, AsyncOperation::kInit,
        base::BindOnce(
            [](base::WeakPtr<SubscriptionsManager> manager, bool result) {
              manager->init_succeeded_ = result;
              manager->OnRequestCompletion();
            },
            weak_ptr_factory_.GetWeakPtr())));
  }
  CheckAndProcessRequest();
}

void SubscriptionsManager::CheckAndProcessRequest() {
  if (has_request_running_ || pending_requests_.empty())
    return;

  // If there is no request running, we can start processing next request in the
  // queue.
  has_request_running_ = true;
  std::unique_ptr<Request> request = std::move(pending_requests_.front());
  pending_requests_.pop();
  CHECK(request->type != SubscriptionType::kTypeUnspecified);

  switch (request->operation) {
    case AsyncOperation::kInit:
      ProcessInitRequest(std::move(request));
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

void SubscriptionsManager::ProcessInitRequest(std::unique_ptr<Request> request){
    // TODO: Get all subscriptions from server and sync with local db.
};

void SubscriptionsManager::ProcessSubscribeRequest(
    std::unique_ptr<Request> request) {
  if (!init_succeeded_) {
    std::move(request->callback).Run(false);
    return;
  }
  // TODO: Check local db and send request to server.
}

void SubscriptionsManager::ProcessUnsubscribeRequest(
    std::unique_ptr<Request> request) {
  if (!init_succeeded_) {
    std::move(request->callback).Run(false);
    return;
  }
  // TODO: Check local db and send request to server.
}

}  // namespace commerce
