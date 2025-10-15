// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_SUBSCRIPTION_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_SUBSCRIPTION_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"

namespace one_time_tokens {

// A handle used by a subscriber to identify a subscription in a
// ExpiringSubscriptionManager.
using ExpiringSubscriptionHandle =
    base::StrongAlias<class StrongAliasTag, base::Uuid>;

class ExpiringSubscriptionManagerBase;

// An `ExpiringSubscription` manages the life-cycle of a subscription on the
// subscriber's side. When the `ExpiringSubscription` is destroyed, the
// `ExpiringSubscriptionManager` is informed about this and cancels the
// subscription. If the `ExpiringSubscriptionManager` is destroyed first,
// all operations fail gracefully.
class ExpiringSubscription {
 public:
  // Creates an `ExpiringSubscription` that is not connected to a an
  // `ExpiringSubscriptionManager`. `IsAlive()` returns false for this.
  // The other functions may be called but don't do anything.
  ExpiringSubscription();
  ExpiringSubscription(ExpiringSubscriptionHandle handle,
                       base::WeakPtr<ExpiringSubscriptionManagerBase> manager);
  ExpiringSubscription(const ExpiringSubscription&) = delete;
  ExpiringSubscription& operator=(const ExpiringSubscription&) = delete;
  ExpiringSubscription(ExpiringSubscription&&);
  ExpiringSubscription& operator=(ExpiringSubscription&&);
  ~ExpiringSubscription();

  const ExpiringSubscriptionHandle& handle() const { return handle_; }

  // Returns true if the `ExpiringSubscriptionManager` still exists and the
  // subscription is not expired.
  bool IsAlive() const;
  void Cancel();
  void SetExpirationTime(base::Time new_expiration);

 private:
  ExpiringSubscriptionHandle handle_;
  base::WeakPtr<ExpiringSubscriptionManagerBase> manager_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_SUBSCRIPTION_H_
