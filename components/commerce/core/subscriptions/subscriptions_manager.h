// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_

#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/check.h"

namespace commerce {

enum class SubscriptionType;
struct CommerceSubscription;

class SubscriptionsManager {
 public:
  SubscriptionsManager();
  SubscriptionsManager(const SubscriptionsManager&) = delete;
  SubscriptionsManager& operator=(const SubscriptionsManager&) = delete;
  ~SubscriptionsManager();

  void Subscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  void Unsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

 private:
  enum class AsyncOperation {
    kInit = 0,
    kSubscribe = 1,
    kUnsubscribe = 2,
  };

  struct Request {
    Request(SubscriptionType type,
            AsyncOperation operation,
            base::OnceCallback<void(bool)> callback);
    Request(SubscriptionType type,
            AsyncOperation operation,
            std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
            base::OnceCallback<void(bool)> callback);
    ~Request();

    SubscriptionType type;
    AsyncOperation operation;
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions;
    base::OnceCallback<void(bool)> callback;
  };

  // Fetch all backend subscriptions and sync with local storage. This should
  // only be called on manager instantiation and user primary account changed.
  void InitSubscriptions();

  // Check if there is any request running. If not, process the next request in
  // the queue.
  void CheckAndProcessRequest();

  // On request completion, mark that no request is running and then check next
  // request. This is chained to the main callback when Request object is built.
  void OnRequestCompletion();

  void ProcessSubscribeRequest(std::unique_ptr<Request> request);

  void ProcessUnsubscribeRequest(std::unique_ptr<Request> request);

  void ProcessInitRequest(std::unique_ptr<Request> request);

  // Hold coming requests until previous ones have finished to avoid race
  // conditions.
  std::queue<std::unique_ptr<Request>> pending_requests_;

  // Whether the initialization is successful. If not, all (un)subscribe
  // operations will fail immediately.
  bool init_succeeded_ = false;

  // Whether there is any request running.
  bool has_request_running_ = false;

  base::WeakPtrFactory<SubscriptionsManager> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_
