// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace one_time_tokens {

ExpiringSubscription::ExpiringSubscription() = default;

ExpiringSubscription::ExpiringSubscription(
    ExpiringSubscriptionHandle handle,
    base::WeakPtr<ExpiringSubscriptionManagerBase> manager)
    : handle_(std::move(handle)), manager_(std::move(manager)) {}

ExpiringSubscription::ExpiringSubscription(ExpiringSubscription&& other) {
  *this = std::move(other);
}

ExpiringSubscription::~ExpiringSubscription() {
  if (manager_) {
    manager_->Cancel(handle_);
  }
}

bool ExpiringSubscription::IsAlive() const {
  return manager_ && manager_->Exists(handle_);
}

void ExpiringSubscription::Cancel() {
  if (manager_) {
    manager_->Cancel(handle_);
  }
}

void ExpiringSubscription::SetExpirationTime(base::Time new_expiration) {
  if (manager_) {
    manager_->SetExpirationTime(handle_, new_expiration);
  }
}

ExpiringSubscription& ExpiringSubscription::operator=(
    ExpiringSubscription&& other) {
  if (manager_) {
    manager_->Cancel(handle_);
  }
  handle_ = std::move(other.handle_);
  manager_ = other.manager_;
  return *this;
}

}  // namespace one_time_tokens
