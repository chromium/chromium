// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"

namespace one_time_tokens {

FakeGmailOtpBackend::FakeGmailOtpBackend()
    : one_time_token_cache_(kGmailTokenCacheDuration,
                            &OneTimeToken::on_device_arrival_time,
                            OneTimeTokenCacheProjection()) {}

FakeGmailOtpBackend::~FakeGmailOtpBackend() = default;

void FakeGmailOtpBackend::SetLogSink(OneTimeTokenLogSink* log_sink) {
  log_sink_ = log_sink;
}

OneTimeTokenLogSink* FakeGmailOtpBackend::GetLogSink() const {
  return log_sink_;
}

ExpiringSubscription FakeGmailOtpBackend::Subscribe(base::Time expiration,
                                                    Callback callback) {
  return Subscribe(expiration, std::move(callback),
                   /*expiration_callback=*/base::DoNothing());
}

ExpiringSubscription FakeGmailOtpBackend::Subscribe(
    base::Time expiration,
    Callback callback,
    base::OnceClosure expiration_callback) {
  if (pending_error_.has_value()) {
    OneTimeTokenRetrievalError error = *pending_error_;
    pending_error_.reset();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(callback, base::unexpected(error)));
  }
  return subscription_manager_.Subscribe(expiration, std::move(callback),
                                         std::move(expiration_callback));
}

ExpiringSubscription FakeGmailOtpBackend::SubscribeToTickles(
    base::Time expiration,
    TickleCallback callback) {
  return tickle_subscription_manager_.Subscribe(
      expiration, std::move(callback),
      /*expiration_callback=*/base::DoNothing());
}

std::vector<OneTimeToken> FakeGmailOtpBackend::GetCachedOneTimeTokens() const {
  return base::ToVector(one_time_token_cache_.GetItems());
}

std::vector<OneTimeToken>
FakeGmailOtpBackend::PurgeExpiredAndGetCachedOneTimeTokens() {
  return base::ToVector(one_time_token_cache_.PurgeExpiredAndGetItems());
}

void FakeGmailOtpBackend::OnIncomingOneTimeTokenBackendNotification(
    const OneTimeTokenBackendNotification& notification) {
  incoming_notifications_.push_back(notification);
  tickle_subscription_manager_.Notify();
}

void FakeGmailOtpBackend::FetchUserDataProcessingConsent(
    FetchUserDataProcessingConsentCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), consent_states_));
}

bool FakeGmailOtpBackend::HasPendingRequests() const {
  return has_pending_requests_;
}

void FakeGmailOtpBackend::ProcessCallbacks(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  if (reply.has_value()) {
    one_time_token_cache_.PurgeExpiredAndAdd(*reply);
  } else if (subscription_manager_.GetNumberSubscribers() == 0) {
    pending_error_ = reply.error();
  }
  subscription_manager_.Notify(reply);
}

void FakeGmailOtpBackend::SetHasPendingRequests(bool has_pending_requests) {
  has_pending_requests_ = has_pending_requests;
}

void FakeGmailOtpBackend::SetUserDataProcessingConsentStates(
    std::optional<UserDataProcessingConsentStates> states) {
  consent_states_ = states;
}

void FakeGmailOtpBackend::ClearCache() {
  one_time_token_cache_.TakeItems();
}

}  // namespace one_time_tokens
