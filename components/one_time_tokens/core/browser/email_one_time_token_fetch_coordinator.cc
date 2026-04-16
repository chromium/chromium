// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_coordinator.h"

namespace one_time_tokens {

EmailOneTimeTokenFetchCoordinator::EmailOneTimeTokenFetchCoordinator(
    Delegate& delegate)
    : delegate_(delegate) {}

EmailOneTimeTokenFetchCoordinator::~EmailOneTimeTokenFetchCoordinator() =
    default;

void EmailOneTimeTokenFetchCoordinator::SignalNetworkRequestNeeded(
    const EncryptedMessageReference& reference) {
  // TODO(crbug.com/478840436): Replace pass-through with real implementation.
  delegate_->OnCanSendNetworkRequest(reference);
}

void EmailOneTimeTokenFetchCoordinator::InformOfNetworkRequestFinished(
    const EncryptedMessageReference& reference) {
  // TODO(crbug.com/478840436): Implement.
}

}  // namespace one_time_tokens
