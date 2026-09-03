// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include <utility>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/email_one_time_token_fetcher.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_constants.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_fetcher.h"
#include "components/one_time_tokens/core/browser/util/expiring_cache.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
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

void GmailOtpBackendImpl::SetLogSink(OneTimeTokenLogSink* log_sink) {
  log_sink_ = log_sink;
}

OneTimeTokenLogSink* GmailOtpBackendImpl::GetLogSink() const {
  return log_sink_;
}

ExpiringSubscription GmailOtpBackendImpl::Subscribe(base::Time expiration,
                                                    Callback callback) {
  ExpiringSubscription subscription = subscription_manager_.Subscribe(
      expiration, std::move(callback),
      /*expiration_callback=*/base::DoNothing());
  if (!url_loader_factory_) {
    LOG_OTT(log_sink_) << "Subscription failed: SharedURLLoaderFactory is null "
                          "(backend initialization failed)";
    base::UmaHistogramBoolean("Autofill.OneTimeTokens.Backend.Gmail.Success",
                              false);
    base::UmaHistogramEnumeration(
        "Autofill.OneTimeTokens.Backend.Gmail.ErrorCode",
        OneTimeTokenRetrievalError::kGmailOtpBackendInitializationFailed);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<GmailOtpBackendImpl> self) {
                         if (self) {
                           self->subscription_manager_.Notify(base::unexpected(
                               OneTimeTokenRetrievalError::
                                   kGmailOtpBackendInitializationFailed));
                         }
                       },
                       weakptr_factory_.GetWeakPtr()));
  } else {
    ProcessCachedNotifications();
  }
  return subscription;
}

ExpiringSubscription GmailOtpBackendImpl::SubscribeToTickles(
    base::Time expiration,
    TickleCallback callback) {
  ExpiringSubscription subscription = tickle_subscription_manager_.Subscribe(
      expiration, callback, /*expiration_callback=*/base::DoNothing());

  // If there are unexpired notifications in the cache, notify the new
  // subscriber immediately about the pre-arrival tickle.
  if (!notification_cache_.PurgeExpiredAndGetItems().empty()) {
    callback.Run();
  }

  return subscription;
}

void GmailOtpBackendImpl::OnIncomingOneTimeTokenBackendNotification(
    const OneTimeTokenBackendNotification& notification) {
  base::UmaHistogramBoolean(
      "Autofill.OneTimeTokens.Backend.Gmail.HasActiveSubscription",
      subscription_manager_.GetNumberSubscribers() > 0 ||
          tickle_subscription_manager_.GetNumberSubscribers() > 0);

  LOG_OTT(log_sink_) << "Tickle received";

  if (base::TimeTicks::Now() - notification.notification_received_timeticks >
      kNotificationExpirationDuration) {
    LOG_OTT(log_sink_) << "Incoming tickle ignored: expired";
    if (base::FeatureList::IsEnabled(features::kGmailOtpRetrievalService)) {
      base::UmaHistogramEnumeration(kTickleArrivalHistogram,
                                    TickleArrival::kExpiredOnArrival);
    }
    return;
  }

  tickle_subscription_manager_.Notify();

  if (active_fetchers_.contains(notification.encrypted_message_reference) ||
      !notification_cache_.PurgeExpiredAndAdd(notification)) {
    LOG_OTT(log_sink_) << "Incoming tickle ignored: duplicate";
    return;
  }

  LOG_OTT(log_sink_) << "Incoming tickle accepted into notification cache";
  ProcessCachedNotifications();
}

bool GmailOtpBackendImpl::HasPendingRequests() const {
  return !notification_cache_.GetItems().empty() ||
         (coordinator_ && coordinator_->HasPendingRequests());
}

void GmailOtpBackendImpl::FetchUserDataProcessingConsent(
    FetchUserDataProcessingConsentCallback callback) {
  LOG_OTT(log_sink_) << "Fetching user data processing consent.";
  pending_consent_callbacks_.push_back(std::move(callback));
  if (consent_fetcher_) {
    LOG_OTT(log_sink_) << "Consent fetch already in flight, queuing callback.";
    return;
  }
  consent_fetcher_ = std::make_unique<UserDataProcessingConsentFetcher>(
      url_loader_factory_, *identity_manager_, log_sink_);
  consent_fetcher_->Start(
      base::BindOnce(&GmailOtpBackendImpl::OnUserDataProcessingConsentFetched,
                     weakptr_factory_.GetWeakPtr()));
}

void GmailOtpBackendImpl::ProcessCachedNotifications() {
  if (subscription_manager_.GetNumberSubscribers() == 0) {
    LOG_OTT(log_sink_)
        << "Skipping processing of cached notifications: no subscribers.";
    return;
  }
  auto items = notification_cache_.TakeItems();
  LOG_OTT(log_sink_) << "Processing " << items.size()
                     << " cached notification(s) for active subscribers.";
  for (const auto& notification : base::Reversed(items)) {
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
    LOG_OTT(log_sink_) << "Aborting Gmail OTP retrieval: all subscribers "
                          "expired or unsubscribed.";
    coordinator_->InformOfNetworkRequestFinished(notification);
    return;
  }

  auto [it, inserted] =
      active_fetchers_.try_emplace(notification.encrypted_message_reference);
  CHECK(inserted);

  LOG_OTT(log_sink_) << "Starting EmailOneTimeTokenFetcher for notification.";
  // TODO(b/543374607): Consider using email_received_timestamp as the source of
  // truth instead.
  it->second = std::make_unique<EmailOneTimeTokenFetcher>(
      url_loader_factory_, *identity_manager_,
      notification.encrypted_message_reference.value(),
      notification.notification_received_timeticks, log_sink_);

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
    base::UmaHistogramEnumeration(
        "Autofill.OneTimeTokens.Backend.Gmail.ErrorCode", reply.error());
  }

  active_fetchers_.erase(notification.encrypted_message_reference);
  coordinator_->InformOfNetworkRequestFinished(notification);

  if (!reply.has_value()) {
    LOG_OTT(log_sink_) << "Gmail OTP backend retrieval failed: error="
                       << std::to_underlying(reply.error())
                       << ". Notifying subscribers.";
    subscription_manager_.Notify(base::unexpected(reply.error()));
    return;
  }

  const OneTimeToken& token = reply.value();
  LOG_OTT(log_sink_) << "Gmail OTP backend retrieval succeeded. Notifying "
                        "subscribers.";
  subscription_manager_.Notify(base::ok(token));
}

void GmailOtpBackendImpl::OnUserDataProcessingConsentFetched(
    std::optional<UserDataProcessingConsentStates> states) {
  LOG_OTT(log_sink_) << "Consent fetch completed: "
                     << (states.has_value() ? "success" : "failed");
  consent_fetcher_.reset();
  std::vector<FetchUserDataProcessingConsentCallback> callbacks =
      std::move(pending_consent_callbacks_);
  pending_consent_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run(states);
  }
}

}  // namespace one_time_tokens
