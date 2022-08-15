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
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace commerce {

class SubscriptionsServerProxy;
class SubscriptionsStorage;
enum class SubscriptionType;
struct CommerceSubscription;

class SubscriptionsManager : public signin::IdentityManager::Observer {
 public:
  SubscriptionsManager(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionProtoStorage<
          commerce_subscription_db::CommerceSubscriptionContentProto>*
          subscription_proto_db);
  // Used for tests. The passed in objects are ordinarily created with
  // parameters from the non-test constructor.
  SubscriptionsManager(signin::IdentityManager* identity_manager,
                       std::unique_ptr<SubscriptionsServerProxy> server_proxy,
                       std::unique_ptr<SubscriptionsStorage> storage);
  SubscriptionsManager(const SubscriptionsManager&) = delete;
  SubscriptionsManager& operator=(const SubscriptionsManager&) = delete;
  ~SubscriptionsManager() override;

  void Subscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  void Unsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  // For tests only, return init_succeeded_.
  bool GetInitSucceededForTesting();

  // For tests only, set has_request_running_.
  void SetHasRequestRunningForTesting(bool has_request_running);

  // For tests only, return whether there are any pending requests.
  bool HasPendingRequestsForTesting();

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
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&);
    Request& operator=(Request&&) = default;
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

  void ProcessSubscribeRequest(Request request);

  void ProcessUnsubscribeRequest(Request request);

  void ProcessInitRequest(Request request);

  void GetRemoteSubscriptionsAndUpdateStorage(
      SubscriptionType type,
      base::OnceCallback<void(bool)> callback);

  void HandleManageSubscriptionsResponse(
      SubscriptionType type,
      base::OnceCallback<void(bool)> callback,
      bool succeeded);

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Hold coming requests until previous ones have finished to avoid race
  // conditions.
  std::queue<Request> pending_requests_;

  // Whether the initialization is successful. If not, all (un)subscribe
  // operations will fail immediately.
  bool init_succeeded_ = false;

  // Whether there is any request running.
  bool has_request_running_ = false;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  std::unique_ptr<SubscriptionsServerProxy> server_proxy_;

  std::unique_ptr<SubscriptionsStorage> storage_;

  base::WeakPtrFactory<SubscriptionsManager> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_
