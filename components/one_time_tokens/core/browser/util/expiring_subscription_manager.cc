// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

#include <optional>
#include <utility>
#include <vector>

namespace one_time_tokens {

namespace internal {
ExpiringSubscriptionDataBase::ExpiringSubscriptionDataBase() = default;
ExpiringSubscriptionDataBase::~ExpiringSubscriptionDataBase() = default;
}  // namespace internal

ExpiringSubscriptionManagerBase::ExpiringSubscriptionManagerBase() = default;
ExpiringSubscriptionManagerBase::~ExpiringSubscriptionManagerBase() = default;

bool ExpiringSubscriptionManagerBase::Exists(
    const ExpiringSubscriptionHandle& handle) const {
  return subscriptions_.contains(handle);
}

[[nodiscard]] base::Time ExpiringSubscriptionManagerBase::GetExpirationTime(
    const ExpiringSubscriptionHandle& handle) const {
  if (auto iter = subscriptions_.find(handle); iter != subscriptions_.end()) {
    return iter->second->expiration;
  }
  return base::Time();
}

void ExpiringSubscriptionManagerBase::SetExpirationTime(
    const ExpiringSubscriptionHandle& handle,
    base::Time new_expiration) {
  if (auto iter = subscriptions_.find(handle); iter != subscriptions_.end()) {
    if (iter->second->expiration == new_expiration) {
      return;
    }
    expiration_queue_.erase({iter->second->expiration, handle});
    iter->second->expiration = new_expiration;
    expiration_queue_.insert({new_expiration, handle});
    UpdateNextExpirationTimer();
  }
}

void ExpiringSubscriptionManagerBase::Cancel(
    const ExpiringSubscriptionHandle& handle) {
  if (auto iter = subscriptions_.find(handle); iter != subscriptions_.end()) {
    expiration_queue_.erase({iter->second->expiration, handle});
    subscriptions_.erase(iter);
    UpdateNextExpirationTimer();
  }
}

size_t ExpiringSubscriptionManagerBase::GetNumberSubscribers() const {
  return subscriptions_.size();
}

void ExpiringSubscriptionManagerBase::ProcessExpirations() {
  const base::Time now = base::Time::Now();

  std::vector<base::OnceClosure> expiration_callbacks;
  expiration_callbacks.reserve(expiration_queue_.size());
  auto expired_end = expiration_queue_.begin();
  while (expired_end != expiration_queue_.end() &&
         expired_end->expiration <= now) {
    if (auto iter = subscriptions_.find(expired_end->handle);
        iter != subscriptions_.end()) {
      if (iter->second->expiration_callback) {
        expiration_callbacks.push_back(
            std::move(iter->second->expiration_callback));
      }
      subscriptions_.erase(iter);
    }
    ++expired_end;
  }
  expiration_queue_.erase(expiration_queue_.begin(), expired_end);

  // Update internal state before invoking external callbacks. An invoked
  // callback could potentially destroy the `ExpiringSubscriptionManager`
  // instance, leading to a Use-After-Free if we accessed `this` afterwards.
  UpdateNextExpirationTimer();

  for (auto& callback : expiration_callbacks) {
    std::move(callback).Run();
  }
}

void ExpiringSubscriptionManagerBase::UpdateNextExpirationTimer() {
  if (expiration_queue_.empty()) {
    next_expected_expiration_ = std::nullopt;
    next_expiration_timer_.Stop();
    return;
  }
  const base::Time next_expiration = expiration_queue_.begin()->expiration;
  if (next_expiration != next_expected_expiration_ ||
      !next_expiration_timer_.IsRunning()) {
    next_expected_expiration_ = next_expiration;
    base::TimeDelta time_until_next_expiration =
        next_expiration - base::Time::Now();
    if (time_until_next_expiration.is_negative()) {
      time_until_next_expiration = base::TimeDelta();
    }
    next_expiration_timer_.Start(
        FROM_HERE, time_until_next_expiration,
        // This is safe because ExpiringSubscriptionManagerBase owns the
        // next_expiration_timer_.
        base::BindOnce(&ExpiringSubscriptionManagerBase::ProcessExpirations,
                       base::Unretained(this)));
  }
}

}  // namespace one_time_tokens
