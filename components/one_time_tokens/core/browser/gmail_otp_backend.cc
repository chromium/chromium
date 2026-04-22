// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/email_one_time_token_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace one_time_tokens {

GmailOtpBackend::~GmailOtpBackend() = default;

// static
std::unique_ptr<GmailOtpBackend> GmailOtpBackend::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager) {
  return std::make_unique<GmailOtpBackendImpl>(std::move(url_loader_factory),
                                               identity_manager);
}

GmailOtpBackendImpl::GmailOtpBackendImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      coordinator_(std::make_unique<EmailOneTimeTokenFetchCoordinator>(*this)) {
}

GmailOtpBackendImpl::~GmailOtpBackendImpl() = default;

ExpiringSubscription GmailOtpBackendImpl::Subscribe(base::Time expiration,
                                                    Callback callback) {
  // TODO(crbug.com/478840436): To preserve the general contract, adding a new
  // subscriber should check a cache if any recent tickles arrived and request
  // OTPs them immediately - as if those tickles arrived just after the
  // subscription.
  return subscription_manager_.Subscribe(expiration, std::move(callback));
}

void GmailOtpBackendImpl::OnIncomingOneTimeTokenBackendNotification(
    const OneTimeTokenBackendNotification& notification) {
  coordinator_->SignalNetworkRequestNeeded(notification);
}

void GmailOtpBackendImpl::OnCanSendNetworkRequest(
    const OneTimeTokenBackendNotification& notification) {
  RetrieveGmailOtp(notification);
}

void GmailOtpBackendImpl::RetrieveGmailOtp(
    const OneTimeTokenBackendNotification& notification) {
  if (subscription_manager_.GetNumberSubscribers() == 0) {
    coordinator_->InformOfNetworkRequestFinished(notification);
    return;
  }

  auto [it, inserted] =
      active_fetchers_.try_emplace(notification.encrypted_message_reference);
  CHECK(inserted);

  it->second = std::make_unique<EmailOneTimeTokenFetcher>(
      url_loader_factory_, *identity_manager_,
      notification.encrypted_message_reference.value());

  it->second->Start(
      base::BindOnce(&GmailOtpBackendImpl::OnResponseFromGmailOtpBackend,
                     weakptr_factory_.GetWeakPtr(), notification));
}

void GmailOtpBackendImpl::OnResponseFromGmailOtpBackend(
    const OneTimeTokenBackendNotification& notification,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  active_fetchers_.erase(notification.encrypted_message_reference);
  coordinator_->InformOfNetworkRequestFinished(notification);

  if (!reply.has_value()) {
    subscription_manager_.Notify(base::unexpected(reply.error()));
    return;
  }

  const OneTimeToken& token = reply.value();
  subscription_manager_.Notify(base::ok(token));
}

}  // namespace one_time_tokens
