// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_coordinator.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"

namespace one_time_tokens {

namespace {

std::string EncodeMessageReference(const EncryptedMessageReference& reference) {
  std::string encoded_reference;
  base::Base64UrlEncode(reference.value(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_reference);
  return encoded_reference;
}

}  // namespace

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
    LOG_OTT(delegate_->GetLogSink())
        << "Coordinator ignoring notification: Duplicate "
           "notification received for an active request: "
        << EncodeMessageReference(notification.encrypted_message_reference);
    return;
  }

  auto it = std::ranges::find_if(pending_queue_, [&](const auto& pending) {
    return pending.notification.encrypted_message_reference ==
           notification.encrypted_message_reference;
  });
  if (it != pending_queue_.end()) {
    LOG_OTT(delegate_->GetLogSink())
        << "Coordinator ignoring notification: Duplicate "
           "notification received for a pending request: "
        << EncodeMessageReference(notification.encrypted_message_reference);
    return;
  }

  pending_queue_.push_back({notification, base::TimeTicks::Now()});
  LOG_OTT(delegate_->GetLogSink())
      << "Notification queued in coordinator. Current queue size: "
      << pending_queue_.size() << ", message_reference: "
      << EncodeMessageReference(notification.encrypted_message_reference);
  base::UmaHistogramCounts100("Autofill.OneTimeTokens.Backend.Gmail.QueueSize",
                              pending_queue_.size());
  ProcessQueue();
}

void EmailOneTimeTokenFetchCoordinator::InformOfNetworkRequestFinished(
    const OneTimeTokenBackendNotification& notification) {
  active_requests_.erase(notification.encrypted_message_reference);
  ProcessQueue();
}

bool EmailOneTimeTokenFetchCoordinator::HasPendingRequests() const {
  return !active_requests_.empty() || !pending_queue_.empty();
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
    QueuedNotification queued_item = std::move(pending_queue_.front());
    pending_queue_.pop_front();

    base::UmaHistogramTimes("Autofill.OneTimeTokens.Backend.Gmail.QueueLatency",
                            base::TimeTicks::Now() - queued_item.entry_time);

    auto [it, inserted] = active_requests_.insert(
        {queued_item.notification.encrypted_message_reference,
         std::move(queued_item.notification)});
    if (inserted) {
      LOG_OTT(delegate_->GetLogSink())
          << "Coordinator starting network request. Active "
             "requests count: "
          << active_requests_.size();
      delegate_->OnCanSendNetworkRequest(it->second, queued_item.entry_time);
    }

    // Check if the delegate callback destroyed this object.
    if (!weak_ptr) {
      return;
    }
  }

  is_processing_queue_ = false;
}

}  // namespace one_time_tokens
