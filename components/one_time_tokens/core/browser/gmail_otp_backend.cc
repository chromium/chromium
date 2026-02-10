// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace one_time_tokens {

GmailOtpBackend::~GmailOtpBackend() = default;

// static
std::unique_ptr<GmailOtpBackend> GmailOtpBackend::Create() {
  return std::make_unique<GmailOtpBackendImpl>();
}

GmailOtpBackendImpl::GmailOtpBackendImpl() = default;
GmailOtpBackendImpl::~GmailOtpBackendImpl() = default;

ExpiringSubscription GmailOtpBackendImpl::Subscribe(base::Time expiration,
                                                    Callback callback) {
  ExpiringSubscription subscription =
      subscription_manager_.Subscribe(expiration, std::move(callback));
  RetrieveGmailOtpIfNeeded();
  return subscription;
}

void GmailOtpBackendImpl::RetrieveGmailOtpIfNeeded() {
  if (has_pending_request_ || !subscription_manager_.GetNumberSubscribers()) {
    return;
  }

  has_pending_request_ = true;
  // TODO(crbug.com/463944653): Replace with a real implementation.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GmailOtpBackendImpl::OnResponseFromGmailOtpBackend,
                     weakptr_factory_.GetWeakPtr(),
                     base::ok(OneTimeToken(OneTimeTokenType::kGmail, "123456",
                                           base::Time::Now()))));
}

void GmailOtpBackendImpl::OnResponseFromGmailOtpBackend(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  has_pending_request_ = false;
  if (!reply.has_value()) {
    subscription_manager_.Notify(base::unexpected(reply.error()));
    return;
  }

  const OneTimeToken& token = reply.value();
  subscription_manager_.Notify(base::ok(token));
}

void GmailOtpBackendImpl::OnIncomingOneTimeTokenBackendTickle(
    const GmailOtpBackendImpl::EncryptedMessageReference&
        encrypted_message_reference) {}

}  // namespace one_time_tokens
