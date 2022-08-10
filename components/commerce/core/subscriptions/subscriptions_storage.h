// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_STORAGE_H_

#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/check.h"

namespace commerce {

enum class SubscriptionType;
struct CommerceSubscription;

using GetLocalSubscriptionsCallback = base::OnceCallback<void(
    std::unique_ptr<std::vector<CommerceSubscription>>)>;

class SubscriptionsStorage {
 public:
  SubscriptionsStorage();
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
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions);

  // Delete all local subscriptions.
  virtual void DeleteAll();
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_STORAGE_H_
