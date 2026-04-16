// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_COORDINATOR_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_COORDINATOR_H_

#include "base/memory/raw_ref.h"
#include "components/one_time_tokens/core/browser/encrypted_message_reference.h"

namespace one_time_tokens {

// Coordinates the lifecycle of EmailOneTimeToken requests, including
// concurrency control and de-duplication.
class EmailOneTimeTokenFetchCoordinator {
 public:
  // Delegate interface to be implemented by GmailOtpBackendImpl.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the coordinator authorizes a network request to be sent.
    virtual void OnCanSendNetworkRequest(
        const EncryptedMessageReference& reference) = 0;
  };

  explicit EmailOneTimeTokenFetchCoordinator(Delegate& delegate);
  EmailOneTimeTokenFetchCoordinator(const EmailOneTimeTokenFetchCoordinator&) =
      delete;
  EmailOneTimeTokenFetchCoordinator& operator=(
      const EmailOneTimeTokenFetchCoordinator&) = delete;
  ~EmailOneTimeTokenFetchCoordinator();

  // Signals that a network request is needed for a specific reference.
  void SignalNetworkRequestNeeded(const EncryptedMessageReference& reference);

  // Informs the coordinator that a network request for a specific reference
  // has finished.
  void InformOfNetworkRequestFinished(
      const EncryptedMessageReference& reference);

 private:
  const raw_ref<Delegate> delegate_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_COORDINATOR_H_
