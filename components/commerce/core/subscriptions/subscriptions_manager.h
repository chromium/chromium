// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
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

class SubscriptionsObserver;
class SubscriptionsServerProxy;
class SubscriptionsStorage;
enum class SubscriptionType;
struct CommerceSubscription;

extern const char kTrackResultHistogramName[];
extern const char kUntrackResultHistogramName[];

// Possible result status of a product (un)tracking request. This enum needs to
// match the values in enums.xml.
enum class SubscriptionsRequestStatus {
  // Subscriptions successfully added or removed on server.
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
  // No action taken because the product is already tracked/untracked on the
  // server.
  kNoOp = 7,

  // This enum must be last and is only used for histograms.
  kMaxValue = kNoOp
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

  // Check if a |subscription| exists in the local database.
  void IsSubscribed(CommerceSubscription subscription,
                    base::OnceCallback<void(bool)> callback);

  // Checks if a subscription exists from the in-memory cache. Use of the the
  // callback-based version |IsSubscribed| is preferred. Information provided
  // by this API is not guaranteed to be correct as it doesn't query the
  // backend.
  bool IsSubscribedFromCache(const CommerceSubscription& subscription);

  // Get all subscriptions that match the provided |type|.
  void GetAllSubscriptions(
      SubscriptionType type,
      base::OnceCallback<void(std::vector<CommerceSubscription>)> callback);

  // On bookmark meta info change, we check its |last_subscription_change_time|
  // against last time we sync server subscriptions with local cache. If the
  // latter one is older, the local cache is outdated and we need to fetch the
  // newest subscriptions from server. This is mainly used to keep local
  // subscriptions up to date when users operate on multiple devices.
  virtual void CheckTimestampOnBookmarkChange(
      int64_t bookmark_subscription_change_time);

  void AddObserver(SubscriptionsObserver* observer);
  void RemoveObserver(SubscriptionsObserver* observer);

  // For tests only, return last_sync_succeeded_.
  bool GetLastSyncSucceededForTesting();

  // For tests only, return last_sync_time_;
  int64_t GetLastSyncTimeForTesting();

  // For tests only, set has_request_running_.
  void SetHasRequestRunningForTesting(bool has_request_running);

  // For tests only, return whether there are any pending requests.
  bool HasPendingRequestsForTesting();

  void SetLastRequestStartedTimeForTesting(base::Time time);

 protected:
  // Default constructor for testing.
  SubscriptionsManager();

 private:
  enum class AsyncOperation {
    kSync = 0,
    kSubscribe = 1,
    kUnsubscribe = 2,
    kLookupOne = 3,
    kGetAll = 4,
    kCheckOnBookmarkChange = 5,
  };

  struct Request {
    Request(AsyncOperation operation, base::OnceCallback<void()> callback);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&);
    Request& operator=(Request&&) = default;
    ~Request();

    AsyncOperation operation;
    base::OnceCallback<void()> callback;
  };

  // Fetch all backend subscriptions and sync with local storage.
  void SyncSubscriptions();

  // Check if there is any request running. If not, process the next request in
  // the queue.
  void CheckAndProcessRequest();

  // Before adding certain operations (Subscribe, Unsubscribe, LookupOne, and
  // GetAll) to the pending requests, if the last sync with server failed, we
  // should re-try the sync first.
  void SyncIfNeeded();

  // On request completion, mark that no request is running and then check next
  // request. This must be chained to the end of callback in every Request.
  void OnRequestCompletion();

  // In certain operations (Sync, Subscribe, Unsubscribe, and
  // CheckOnBookmarkChange), we may sync local cache with server and need to
  // update |last_sync_succeeded_| and |last_sync_time_|.
  void UpdateSyncStates(bool sync_succeeded);

  void HandleSync();

  void OnSyncStatusFetched(SubscriptionsRequestStatus result);

  void HandleSubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  void OnSubscribeStatusFetched(
      std::vector<CommerceSubscription> notified_subscriptions,
      base::OnceCallback<void(bool)> callback,
      SubscriptionsRequestStatus result);

  void OnIncomingSubscriptionsFilteredForSubscribe(
      SubscriptionType type,
      SubscriptionsRequestCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> unique_subscriptions);

  void HandleUnsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  void OnUnsubscribeStatusFetched(
      std::vector<CommerceSubscription> notified_subscriptions,
      base::OnceCallback<void(bool)> callback,
      SubscriptionsRequestStatus result);

  void OnIncomingSubscriptionsFilteredForUnsubscribe(
      SubscriptionType type,
      SubscriptionsRequestCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> unique_subscriptions);

  void GetRemoteSubscriptionsAndUpdateStorage(
      SubscriptionType type,
      SubscriptionsRequestCallback callback);

  void HandleGetSubscriptionsResponse(
      SubscriptionType type,
      SubscriptionsRequestCallback callback,
      SubscriptionsRequestStatus status,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  void HandleManageSubscriptionsResponse(
      SubscriptionType type,
      SubscriptionsRequestCallback callback,
      SubscriptionsRequestStatus status,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  void HandleLookup(CommerceSubscription subscription,
                    base::OnceCallback<void(bool)> callback);

  void OnLookupResult(base::OnceCallback<void(bool)> callback,
                      bool is_subscribed);

  void HandleGetAll(
      SubscriptionType type,
      base::OnceCallback<void(std::vector<CommerceSubscription>)> callback);

  void OnGetAllResult(
      base::OnceCallback<void(std::vector<CommerceSubscription>)> callback,
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions);

  void HandleCheckTimestampOnBookmarkChange(
      int64_t bookmark_subscription_change_time);

  void HandleGetSubscriptionsResponseOnBookmarkChange(
      SubscriptionsRequestStatus status,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  void OnStorageUpdatedOnBookmarkChange(
      SubscriptionsRequestStatus status,
      std::vector<CommerceSubscription> added_subs,
      std::vector<CommerceSubscription> removed_subs);

  void OnSubscribe(const std::vector<CommerceSubscription>& subscriptions,
                   bool succeeded);
  void OnUnsubscribe(const std::vector<CommerceSubscription>& subscriptions,
                     bool succeeded);

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  bool HasRequestRunning();

  // Hold coming requests until previous ones have finished to avoid race
  // conditions.
  std::queue<Request> pending_requests_;

  // Whether the last sync with server is successful. If not, all (un)subscribe
  // operations will fail immediately.
  bool last_sync_succeeded_ = false;
  // Last time we successfully synced with server.
  int64_t last_sync_time_{0L};

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

  base::ObserverList<SubscriptionsObserver>::Unchecked observers_;

  base::WeakPtrFactory<SubscriptionsManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_MANAGER_H_
