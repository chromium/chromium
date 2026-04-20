// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_COORDINATOR_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_COORDINATOR_H_

#include <deque>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"

namespace one_time_tokens {

// Coordinates the lifecycle of EmailOneTimeToken requests, including
// concurrency control and de-duplication.
class EmailOneTimeTokenFetchCoordinator {
 public:
  static constexpr size_t kMaxConcurrentRequests = 3;

  // Delegate interface to be implemented by GmailOtpBackendImpl.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the coordinator authorizes a network request to be sent.
    virtual void OnCanSendNetworkRequest(
        const OneTimeTokenBackendNotification& notification) = 0;
  };

  explicit EmailOneTimeTokenFetchCoordinator(Delegate& delegate);
  EmailOneTimeTokenFetchCoordinator(const EmailOneTimeTokenFetchCoordinator&) =
      delete;
  EmailOneTimeTokenFetchCoordinator& operator=(
      const EmailOneTimeTokenFetchCoordinator&) = delete;
  ~EmailOneTimeTokenFetchCoordinator();

  // Signals that a network request is needed for a specific notification.
  void SignalNetworkRequestNeeded(
      const OneTimeTokenBackendNotification& notification);

  // Informs the coordinator that a network request for a specific notification
  // has finished.
  void InformOfNetworkRequestFinished(
      const OneTimeTokenBackendNotification& notification);

 private:
  void ProcessQueue();

  const raw_ref<Delegate> delegate_;

  // A queue of notifications waiting to be processed when the number of active
  // requests is below kMaxConcurrentRequests.
  std::deque<OneTimeTokenBackendNotification> pending_queue_;

  // The notifications currently undergoing a network fetch, keyed by their
  // unique encrypted_message_reference.
  base::flat_map<EncryptedMessageReference, OneTimeTokenBackendNotification>
      active_requests_;

  // Set to true while ProcessQueue is running to prevent re-entrancy issues.
  bool is_processing_queue_ = false;

  base::WeakPtrFactory<EmailOneTimeTokenFetchCoordinator> weak_ptr_factory_{
      this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_COORDINATOR_H_
