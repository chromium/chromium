// Copyright 2022 The Chromium Authors
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
#include "base/time/time.h"
#include "components/commerce/core/account_checker.h"
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

// Possible result status of a product (un)tracking request. This enum needs to
// match the values in enums.xml.
enum class SubscriptionsRequestStatus {
  kSuccess = 0,
  // Server failed to parse the request.
  kServerParseError = 1,
  // Server successfully parsed the request, but failed afterwards.
  kServerInternalError = 2,
  // Local storage failed to load, create, or delete subscriptions.
  kStorageError = 3,
  // If the last sync with server failed, we just drop this request.
  kLastSyncFailed = 4,
  // The passed in argument is invalid.
  kInvalidArgument = 5,
  // The request was lost somewhere unknown and never came back. This is used
  // for monitoring purpose only and should never happen if the subscriptions
  // work correctly.
  kLost = 6,

  // This enum must be last and is only used for histograms.
  kMaxValue = kLost
};

using SubscriptionsRequestCallback =
    base::OnceCallback<void(SubscriptionsRequestStatus)>;

class SubscriptionsManager : public signin::IdentityManager::Observer {
 public:
  SubscriptionsManager(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionProtoStorage<
          commerce_subscription_db::CommerceSubscriptionContentProto>*
          subscription_proto_db,
      AccountChecker* account_checker);
  // Used for tests. The passed in objects are ordinarily created with
  // parameters from the non-test constructor.
  SubscriptionsManager(signin::IdentityManager* identity_manager,
                       std::unique_ptr<SubscriptionsServerProxy> server_proxy,
                       std::unique_ptr<SubscriptionsStorage> storage,
                       AccountChecker* account_checker);
  SubscriptionsManager(const SubscriptionsManager&) = delete;
  SubscriptionsManager& operator=(const SubscriptionsManager&) = delete;
  ~SubscriptionsManager() override;

  void Subscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  void Unsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  // If a |subscription| should exist but we cannot find it in local
  // subscriptions, or vice versa, we should sync local subscriptions with the
  // server. This is mainly used to keep local subscriptions up to date when
  // users operate on multiple devices.
  void VerifyIfSubscriptionExists(CommerceSubscription subscription,
                                  bool should_exist);

  // Check if a |subscription| exists in the local database.
  void IsSubscribed(CommerceSubscription subscription,
                    base::OnceCallback<void(bool)> callback);

  // For tests only, return last_sync_succeeded_.
  bool GetLastSyncSucceededForTesting();

  // For tests only, set has_request_running_.
  void SetHasRequestRunningForTesting(bool has_request_running);

  // For tests only, return whether there are any pending requests.
  bool HasPendingRequestsForTesting();

  void SetLastRequestStartedTimeForTesting(base::Time time);

 private:
  enum class AsyncOperation {
    kSync = 0,
    kSubscribe = 1,
    kUnsubscribe = 2,
  };

  struct Request {
    Request(SubscriptionType type,
            AsyncOperation operation,
            SubscriptionsRequestCallback callback);
    Request(SubscriptionType type,
            AsyncOperation operation,
            std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
            SubscriptionsRequestCallback callback);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&);
    Request& operator=(Request&&) = default;
    ~Request();

    SubscriptionType type;
    AsyncOperation operation;
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions;
    SubscriptionsRequestCallback callback;
  };

  // Fetch all backend subscriptions and sync with local storage. This should
  // only be called on manager instantiation and user primary account changed.
  void SyncSubscriptions();

  // Check if there is any request running. If not, process the next request in
  // the queue.
  void CheckAndProcessRequest();

  // On request completion, mark that no request is running and then check next
  // request. This is chained to the main callback when Request object is built.
  void OnRequestCompletion();

  void ProcessSubscribeRequest(Request request);

  void ProcessUnsubscribeRequest(Request request);

  void ProcessSyncRequest(Request request);

  void GetRemoteSubscriptionsAndUpdateStorage(
      SubscriptionType type,
      SubscriptionsRequestCallback callback);

  void HandleGetSubscriptionsResponse(
      SubscriptionType type,
      SubscriptionsRequestCallback callback,
      SubscriptionsRequestStatus status,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  void HandleManageSubscriptionsResponse(SubscriptionType type,
                                         SubscriptionsRequestCallback callback,
                                         SubscriptionsRequestStatus status);

  void HandleCheckLocalSubscriptionResponse(bool should_exisit,
                                            bool is_subscribed);

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  bool HasRequestRunning();

  // Hold coming requests until previous ones have finished to avoid race
  // conditions.
  std::queue<Request> pending_requests_;

  // Whether the last sync with server is successful. If not, all (un)subscribe
  // operations will fail immediately.
  bool last_sync_succeeded_ = false;

  // Whether there is any request running.
  bool has_request_running_ = false;

  base::Time last_request_started_time_ = base::Time();

  AsyncOperation last_request_operation_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  std::unique_ptr<SubscriptionsServerProxy> server_proxy_;

  std::unique_ptr<SubscriptionsStorage> storage_;

  raw_ptr<AccountChecker> account_checker_;

  base::WeakPtrFactory<SubscriptionsManager> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_
