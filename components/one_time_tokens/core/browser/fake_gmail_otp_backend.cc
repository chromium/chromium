// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace one_time_tokens {

FakeGmailOtpBackend::FakeGmailOtpBackend() = default;
FakeGmailOtpBackend::~FakeGmailOtpBackend() = default;

ExpiringSubscription FakeGmailOtpBackend::Subscribe(base::Time expiration,
                                                    Callback callback) {
  callbacks_.push_back(callback);
  return ExpiringSubscription();
}

ExpiringSubscription FakeGmailOtpBackend::SubscribeToTickles(
    base::Time expiration,
    TickleCallback callback) {
  return ExpiringSubscription();
}

std::vector<OneTimeToken> FakeGmailOtpBackend::GetCachedOneTimeTokens() const {
  return {};
}

std::vector<OneTimeToken>
FakeGmailOtpBackend::PurgeExpiredAndGetCachedOneTimeTokens() {
  return {};
}

void FakeGmailOtpBackend::OnIncomingOneTimeTokenBackendNotification(
    const OneTimeTokenBackendNotification& notification) {
  incoming_notifications_.push_back(notification);
}

void FakeGmailOtpBackend::FetchUserDataProcessingConsent(
    FetchUserDataProcessingConsentCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

bool FakeGmailOtpBackend::HasPendingRequests() const {
  return false;
}

void FakeGmailOtpBackend::ProcessCallbacks(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  for (auto& callback : callbacks_) {
    callback.Run(reply);
  }
  callbacks_.clear();
}

}  // namespace one_time_tokens
