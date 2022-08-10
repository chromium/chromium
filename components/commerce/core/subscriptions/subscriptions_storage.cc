// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/check.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"

namespace commerce {

SubscriptionsStorage::SubscriptionsStorage() = default;
SubscriptionsStorage::~SubscriptionsStorage() = default;

// TODO(crbug.com/1329197): Implement these APIs after ProtoDB is moved to
// //components.
void SubscriptionsStorage::GetUniqueNonExistingSubscriptions(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    GetLocalSubscriptionsCallback callback) {}

void SubscriptionsStorage::GetUniqueExistingSubscriptions(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    GetLocalSubscriptionsCallback callback) {}

void SubscriptionsStorage::UpdateStorage(
    SubscriptionType type,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions) {}

void SubscriptionsStorage::DeleteAll() {}

}  // namespace commerce
