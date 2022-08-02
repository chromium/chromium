// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include <queue>
#include <string>

namespace commerce {

SubscriptionsManager::SubscriptionsManager(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : weak_ptr_factory_(this) {
  server_proxy_ = std::make_unique<SubscriptionsServerProxy>(
      identity_manager, std::move(url_loader_factory));
  storage_ = std::make_unique<SubscriptionsStorage>();
// Avoid duplicate server calls on android. Remove this after we integrate
// android implementation to shopping service.
#if !BUILDFLAG(IS_ANDROID)
  InitSubscriptions();
#endif  // !BUILDFLAG(IS_ANDROID)
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
SubscriptionsManager::Request::Request(Request&&) = default;
SubscriptionsManager::Request::~Request() = default;

void SubscriptionsManager::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  SubscriptionType type = (*subscriptions)[0].type;
  pending_requests_.push(
      Request(type, AsyncOperation::kSubscribe, std::move(subscriptions),
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
  pending_requests_.push(
      Request(type, AsyncOperation::kUnsubscribe, std::move(subscriptions),
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
    pending_requests_.push(Request(
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
  Request request = std::move(pending_requests_.front());
  pending_requests_.pop();
  CHECK(request.type != SubscriptionType::kTypeUnspecified);

  switch (request.operation) {
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

void SubscriptionsManager::ProcessInitRequest(Request request) {
  GetRemoteSubscriptionsAndUpdateStorage(request.type,
                                         std::move(request.callback));
};

void SubscriptionsManager::ProcessSubscribeRequest(Request request) {
  if (!init_succeeded_) {
    std::move(request.callback).Run(false);
    return;
  }
  storage_->GetUniqueNonExistingSubscriptions(
      std::move(request.subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager, SubscriptionType type,
             base::OnceCallback<void(bool)> callback,
             std::unique_ptr<std::vector<CommerceSubscription>>
                 unique_subscriptions) {
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
  if (!init_succeeded_) {
    std::move(request.callback).Run(false);
    return;
  }
  storage_->GetUniqueExistingSubscriptions(
      std::move(request.subscriptions),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsManager> manager, SubscriptionType type,
             base::OnceCallback<void(bool)> callback,
             std::unique_ptr<std::vector<CommerceSubscription>>
                 unique_subscriptions) {
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
    base::OnceCallback<void(bool)> callback) {
  server_proxy_->Get(type, base::BindOnce(&SubscriptionsStorage::UpdateStorage,
                                          base::Unretained(storage_.get()),
                                          type, std::move(callback)));
}

void SubscriptionsManager::HandleManageSubscriptionsResponse(
    SubscriptionType type,
    base::OnceCallback<void(bool)> callback,
    bool succeeded) {
  if (!succeeded) {
    VLOG(1) << "Fail to create or delete subscriptions on server";
    std::move(callback).Run(false);
  } else {
    GetRemoteSubscriptionsAndUpdateStorage(type, std::move(callback));
  }
}

}  // namespace commerce
