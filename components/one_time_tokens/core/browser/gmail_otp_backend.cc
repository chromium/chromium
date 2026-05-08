// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/email_one_time_token_fetcher.h"
#include "components/one_time_tokens/core/browser/util/expiring_cache.h"
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
      coordinator_(std::make_unique<EmailOneTimeTokenFetchCoordinator>(*this)),
      notification_cache_(
          kNotificationExpirationDuration,
          &OneTimeTokenBackendNotification::notification_received_timeticks) {}

GmailOtpBackendImpl::~GmailOtpBackendImpl() = default;

ExpiringSubscription GmailOtpBackendImpl::Subscribe(base::Time expiration,
                                                    Callback callback) {
  ExpiringSubscription subscription =
      subscription_manager_.Subscribe(expiration, std::move(callback));
  ProcessCachedNotifications();
  return subscription;
}

void GmailOtpBackendImpl::OnIncomingOneTimeTokenBackendNotification(
    const OneTimeTokenBackendNotification& notification) {
  base::UmaHistogramBoolean(
      "Autofill.OneTimeTokens.Backend.Gmail.HasActiveSubscription",
      subscription_manager_.GetNumberSubscribers() > 0);
  notification_cache_.PurgeExpiredAndAdd(notification);
  ProcessCachedNotifications();
}

void GmailOtpBackendImpl::ProcessCachedNotifications() {
  if (subscription_manager_.GetNumberSubscribers() == 0) {
    return;
  }
  for (const auto& notification : notification_cache_.TakeItems()) {
    base::UmaHistogramMediumTimes(
        "Autofill.OneTimeTokens.Backend.Gmail.SubscriptionWaitLatency",
        base::TimeTicks::Now() - notification.notification_received_timeticks);
    coordinator_->SignalNetworkRequestNeeded(notification);
  }
}

void GmailOtpBackendImpl::OnCanSendNetworkRequest(
    const OneTimeTokenBackendNotification& notification,
    base::TimeTicks trigger_time) {
  RetrieveGmailOtp(notification, trigger_time);
}

void GmailOtpBackendImpl::RetrieveGmailOtp(
    const OneTimeTokenBackendNotification& notification,
    base::TimeTicks trigger_time) {
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

  it->second->Start(base::BindOnce(
      &GmailOtpBackendImpl::OnResponseFromGmailOtpBackend,
      weakptr_factory_.GetWeakPtr(), notification, trigger_time));
}

void GmailOtpBackendImpl::OnResponseFromGmailOtpBackend(
    const OneTimeTokenBackendNotification& notification,
    base::TimeTicks trigger_time,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  base::UmaHistogramBoolean("Autofill.OneTimeTokens.Backend.Gmail.Success",
                            reply.has_value());

  if (reply.has_value()) {
    base::UmaHistogramTimes(
        "Autofill.OneTimeTokens.Backend.Gmail.SuccessLatency",
        base::TimeTicks::Now() - trigger_time);
  } else {
    base::UmaHistogramTimes("Autofill.OneTimeTokens.Backend.Gmail.ErrorLatency",
                            base::TimeTicks::Now() - trigger_time);
  }

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
