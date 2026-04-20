// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_coordinator.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"

namespace one_time_tokens {

EmailOneTimeTokenFetchCoordinator::EmailOneTimeTokenFetchCoordinator(
    Delegate& delegate)
    : delegate_(delegate) {}

EmailOneTimeTokenFetchCoordinator::~EmailOneTimeTokenFetchCoordinator() =
    default;

void EmailOneTimeTokenFetchCoordinator::SignalNetworkRequestNeeded(
    const OneTimeTokenBackendNotification& notification) {
  // Check if we are already tracking this notification in either the active
  // requests or the pending queue.
  if (active_requests_.contains(notification.encrypted_message_reference)) {
    DLOG(WARNING) << "Duplicate notification received for an active request: "
                  << notification.encrypted_message_reference.value();
    return;
  }

  auto it = std::ranges::find_if(pending_queue_, [&](const auto& pending) {
    return pending.encrypted_message_reference ==
           notification.encrypted_message_reference;
  });
  if (it != pending_queue_.end()) {
    DLOG(WARNING) << "Duplicate notification received for a pending request: "
                  << notification.encrypted_message_reference.value();
    return;
  }

  pending_queue_.push_back(notification);
  ProcessQueue();
}

void EmailOneTimeTokenFetchCoordinator::InformOfNetworkRequestFinished(
    const OneTimeTokenBackendNotification& notification) {
  active_requests_.erase(notification.encrypted_message_reference);
  ProcessQueue();
}

void EmailOneTimeTokenFetchCoordinator::ProcessQueue() {
  if (is_processing_queue_) {
    return;
  }

  is_processing_queue_ = true;
  base::WeakPtr<EmailOneTimeTokenFetchCoordinator> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();

  while (active_requests_.size() < kMaxConcurrentRequests &&
         !pending_queue_.empty()) {
    OneTimeTokenBackendNotification notification =
        std::move(pending_queue_.front());
    pending_queue_.pop_front();

    auto [it, inserted] = active_requests_.insert(
        {notification.encrypted_message_reference, std::move(notification)});
    if (inserted) {
      delegate_->OnCanSendNetworkRequest(it->second);
    }

    // Check if the delegate callback destroyed this object.
    if (!weak_ptr) {
      return;
    }
  }

  is_processing_queue_ = false;
}

}  // namespace one_time_tokens
