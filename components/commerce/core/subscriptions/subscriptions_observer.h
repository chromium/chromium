// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_OBSERVER_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_OBSERVER_H_

#include "components/commerce/core/subscriptions/commerce_subscription.h"

namespace commerce {

class SubscriptionsObserver {
 public:
  SubscriptionsObserver(const SubscriptionsObserver&) = delete;
  SubscriptionsObserver& operator=(const SubscriptionsObserver&) = delete;

  // Invoked when a subscribe request for |subscription| has finished.
  virtual void OnSubscribe(const CommerceSubscription& subscription,
                           bool succeeded) = 0;

  // Invoked when an unsubscribe request for |subscription| has finished.
  virtual void OnUnsubscribe(const CommerceSubscription& subscription,
                             bool succeeded) = 0;

 protected:
  SubscriptionsObserver() = default;
  virtual ~SubscriptionsObserver() = default;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_OBSERVER_H_
