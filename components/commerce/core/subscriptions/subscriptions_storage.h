// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_STORAGE_H_

#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

enum class SubscriptionType;
struct CommerceSubscription;

// Used to handle locally fetched subscriptions.
using GetLocalSubscriptionsCallback = base::OnceCallback<void(
    std::unique_ptr<std::vector<CommerceSubscription>>)>;
// Used to handle if storage-related operation succeeds.
using StorageOperationCallback =
    base::OnceCallback<void(SubscriptionsRequestStatus)>;
// Indicate if storage is updated successfully and pass the added & removed
// subscriptions to the callback.
using StorageUpdateCallback =
    base::OnceCallback<void(SubscriptionsRequestStatus,
                            std::vector<CommerceSubscription>,
                            std::vector<CommerceSubscription>)>;

using CommerceSubscriptionProto =
    commerce_subscription_db::CommerceSubscriptionContentProto;
using CommerceSubscriptions =
    std::vector<SessionProtoStorage<CommerceSubscriptionProto>::KeyAndValue>;
using SubscriptionManagementTypeProto = commerce_subscription_db::
    CommerceSubscriptionContentProto_SubscriptionManagementType;
using SubscriptionTypeProto =
    commerce_subscription_db::CommerceSubscriptionContentProto_SubscriptionType;
using TrackingIdTypeProto =
    commerce_subscription_db::CommerceSubscriptionContentProto_TrackingIdType;

class SubscriptionsStorage {
 public:
  explicit SubscriptionsStorage(
      SessionProtoStorage<CommerceSubscriptionProto>* subscription_proto_db);
  SubscriptionsStorage(const SubscriptionsStorage&) = delete;
  SubscriptionsStorage& operator=(const SubscriptionsStorage&) = delete;
  virtual ~SubscriptionsStorage();

  // Compare the provided subscriptions against local cache and return unique
  // subscriptions that are not in local cache. This is used for subscribe
  // operation.
  virtual void GetUniqueNonExistingSubscriptions(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      GetLocalSubscriptionsCallback callback);

  // Compare the provided subscriptions against local cache and return unique
  // subscriptions that are already in local cache. This is used for unsubscribe
  // operation.
  virtual void GetUniqueExistingSubscriptions(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      GetLocalSubscriptionsCallback callback);

  // Update local cache to keep consistency with |remote_subscriptions| and
  // notify |callback| if it completes successfully.
  virtual void UpdateStorage(
      SubscriptionType type,
      StorageOperationCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  // Update local cache to keep consistency with |remote_subscriptions| and
  // notify |callback| if it completes successfully. This will also pass the
  // added & removed subscriptions to the |callback|.
  virtual void UpdateStorageAndNotifyModifiedSubscriptions(
      SubscriptionType type,
      StorageUpdateCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  // Delete all local subscriptions.
  virtual void DeleteAll();

  // Check if the given subscription is in local storage.
  virtual void IsSubscribed(CommerceSubscription subscription,
                            base::OnceCallback<void(bool)> callback);

  // Checks if a subscription exists from the in-memory cache. Use of the
  // callback-based version |IsSubscribed| is preferred. Information provided
  // by this API is not guaranteed to be correct as it doesn't query the
  // backend.
  virtual bool IsSubscribedFromCache(const CommerceSubscription& subscription);

  // Get all subscriptions that match the provided |type|.
  virtual void LoadAllSubscriptionsForType(
      SubscriptionType type,
      GetLocalSubscriptionsCallback callback);

 protected:
  // Default constructor for testing.
  SubscriptionsStorage();

 private:
  void SaveSubscription(const CommerceSubscription& subscription,
                        base::OnceCallback<void(bool)> callback);

  void DeleteSubscription(const CommerceSubscription& subscription,
                          base::OnceCallback<void(bool)> callback);

  CommerceSubscription GetSubscriptionFromProto(
      const SessionProtoStorage<CommerceSubscriptionProto>::KeyAndValue& kv);

  // Convert subscription list to a map keyed by the subscription key to remove
  // duplicates and to easily lookup.
  std::unordered_map<std::string, CommerceSubscription> SubscriptionsListToMap(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions);

  void PerformGetNonExistingSubscriptions(
      std::unique_ptr<std::vector<CommerceSubscription>> incoming_subscriptions,
      GetLocalSubscriptionsCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions);

  void PerformGetExistingSubscriptions(
      std::unique_ptr<std::vector<CommerceSubscription>> incoming_subscriptions,
      GetLocalSubscriptionsCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions);

  void PerformUpdateStorage(
      StorageUpdateCallback callback,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions,
      std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions);

  // Load all subscriptions regardless of type.
  void LoadAllSubscriptions(GetLocalSubscriptionsCallback callback);

  void HandleLoadCompleted(GetLocalSubscriptionsCallback callback,
                           bool succeeded,
                           CommerceSubscriptions data);

  raw_ptr<SessionProtoStorage<CommerceSubscriptionProto>> proto_db_;

  // An in-memory cache of subscriptions that can be accessed synchronously.
  // This may not have the most up-to-date information as it does not check
  // the backend.
  std::unordered_set<std::string> subscriptions_cache_;

  base::WeakPtrFactory<SubscriptionsStorage> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_STORAGE_H_
