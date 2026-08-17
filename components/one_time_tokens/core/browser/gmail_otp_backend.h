// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_

#include <memory>
#include <set>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_coordinator.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/one_time_tokens/core/browser/util/expiring_cache.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace one_time_tokens {

class OneTimeTokenLogSink;

// Duration after which notifications expire and won't be processed.
inline constexpr base::TimeDelta kNotificationExpirationDuration =
    base::Minutes(3);

class EmailOneTimeTokenFetcher;
class UserDataProcessingConsentFetcher;

// Abstract interface for fetching OTPs from Gmail.
class GmailOtpBackend : public KeyedService {
 public:
  using CallbackSignature =
      void(base::expected<OneTimeToken, OneTimeTokenRetrievalError>);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  ~GmailOtpBackend() override;

  // Creates a new instance of the backend.
  static std::unique_ptr<GmailOtpBackend> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager);

  virtual void SetLogSink(OneTimeTokenLogSink* log_sink) {}

  // Creates a subscription for new incoming OTPs.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(base::Time expiration,
                                                       Callback callback) = 0;

  // Called when a new OTP is received via the OneTimeToken notification.
  virtual void OnIncomingOneTimeTokenBackendNotification(
      const OneTimeTokenBackendNotification& notification) = 0;

  using FetchUserDataProcessingConsentCallback =
      base::OnceCallback<void(std::optional<UserDataProcessingConsentStates>)>;
  // Fetches the user data processing consent states from the backend.
  virtual void FetchUserDataProcessingConsent(
      FetchUserDataProcessingConsentCallback callback) = 0;
};

// Concrete implementation of GmailOtpBackend that fetches OTPs and consent
// states from the backend.
class GmailOtpBackendImpl : public GmailOtpBackend,
                            public EmailOneTimeTokenFetchCoordinator::Delegate {
 public:
  GmailOtpBackendImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager);
  ~GmailOtpBackendImpl() override;

  void SetLogSink(OneTimeTokenLogSink* log_sink) override;

  ExpiringSubscription Subscribe(base::Time expiration,
                                 Callback callback) override;

  void OnIncomingOneTimeTokenBackendNotification(
      const OneTimeTokenBackendNotification& notification) override;

  void FetchUserDataProcessingConsent(
      FetchUserDataProcessingConsentCallback callback) override;

  void OnCanSendNetworkRequest(
      const OneTimeTokenBackendNotification& notification,
      base::TimeTicks trigger_time) override;

  OneTimeTokenLogSink* GetLogSink() const override;

 private:
  void ProcessCachedNotifications();

  void RetrieveGmailOtp(const OneTimeTokenBackendNotification& notification,
                        base::TimeTicks trigger_time);

  void OnResponseFromGmailOtpBackend(
      const OneTimeTokenBackendNotification& notification,
      base::TimeTicks trigger_time,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  void OnUserDataProcessingConsentFetched(
      std::optional<UserDataProcessingConsentStates> states);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  raw_ref<signin::IdentityManager> identity_manager_;

  // Handles subscriptions to the `GmailOtpBackend`.
  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;

  // Owned by `OneTimeTokenServiceImpl`, outlives this backend. May be null.
  raw_ptr<OneTimeTokenLogSink> log_sink_ = nullptr;

  // Policy for coordinating network requests.
  std::unique_ptr<EmailOneTimeTokenFetchCoordinator> coordinator_;

  ExpiringCache<
      OneTimeTokenBackendNotification,
      decltype(&OneTimeTokenBackendNotification::
                   notification_received_timeticks),
      OneTimeTokenBackendNotification::EncryptedMessageReferenceProjection>
      notification_cache_;

  // Active fetchers for Gmail OTPs, keyed by their unique
  // encrypted_message_reference.
  base::flat_map<EncryptedMessageReference,
                 std::unique_ptr<EmailOneTimeTokenFetcher>>
      active_fetchers_;

  // Active fetcher for user data processing consent.
  std::unique_ptr<UserDataProcessingConsentFetcher> consent_fetcher_;

  // Pending callbacks for in-flight consent fetch request.
  std::vector<FetchUserDataProcessingConsentCallback>
      pending_consent_callbacks_;

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<GmailOtpBackendImpl> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
