// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace one_time_tokens {

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
    iter->second->expiration = new_expiration;
    UpdateNextExpirationTimer();
  }
}

void ExpiringSubscriptionManagerBase::Cancel(
    const ExpiringSubscriptionHandle& handle) {
  if (auto iter = subscriptions_.find(handle); iter != subscriptions_.end()) {
    subscriptions_.erase(iter);
    UpdateNextExpirationTimer();
  }
}

size_t ExpiringSubscriptionManagerBase::GetNumberSubscribers() const {
  return subscriptions_.size();
}

void ExpiringSubscriptionManagerBase::ProcessExpirations() {
  const base::Time now = base::Time::Now();

  // Identify all handles of expired subscriptions.
  std::vector<ExpiringSubscriptionHandle> expired_handles;
  expired_handles.reserve(subscriptions_.size());
  for (auto& [handle, subscription] : subscriptions_) {
    if (subscription->expiration <= now) {
      expired_handles.push_back(handle);
    }
  }

  // Remove the expired subscriptions.
  for (const ExpiringSubscriptionHandle& handle : expired_handles) {
    subscriptions_.erase(handle);
  }

  UpdateNextExpirationTimer();
}

void ExpiringSubscriptionManagerBase::UpdateNextExpirationTimer() {
  const std::optional<base::Time> next_expiration = GetNextExpirationTime();
  if (!next_expiration.has_value()) {
    next_expected_expiration_ = std::nullopt;
    next_expiration_timer_.Stop();
    return;
  }
  if (!next_expected_expiration_.has_value() ||
      next_expiration.value() != next_expected_expiration_.value()) {
    base::TimeDelta time_until_next_expiration =
        next_expiration.value() - base::Time::Now();
    next_expected_expiration_ = next_expiration;
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

std::optional<base::Time>
ExpiringSubscriptionManagerBase::GetNextExpirationTime() {
  auto iter = std::ranges::min_element(
      subscriptions_, {},
      [](const auto& pair) { return pair.second->expiration; });
  return iter != subscriptions_.end()
             ? std::make_optional(iter->second->expiration)
             : std::nullopt;
}

}  // namespace one_time_tokens
