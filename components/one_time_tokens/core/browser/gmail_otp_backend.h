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

// Duration after which notifications expire and won't be processed.
inline constexpr base::TimeDelta kNotificationExpirationDuration =
    base::Minutes(3);

class EmailOneTimeTokenFetcher;

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

  // Creates a subscription for new incoming OTPs.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(base::Time expiration,
                                                       Callback callback) = 0;

  // Called when a new OTP is received via the OneTimeToken notification.
  virtual void OnIncomingOneTimeTokenBackendNotification(
      const OneTimeTokenBackendNotification& notification) = 0;
};

// Concrete implementation of GmailOtpBackend that provides a fake OTP
// response. This is intended for use in testing and development environments
// where a real backend is not available.
class GmailOtpBackendImpl : public GmailOtpBackend,
                            public EmailOneTimeTokenFetchCoordinator::Delegate {
 public:
  GmailOtpBackendImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager);
  ~GmailOtpBackendImpl() override;

  ExpiringSubscription Subscribe(base::Time expiration,
                                 Callback callback) override;

  void OnIncomingOneTimeTokenBackendNotification(
      const OneTimeTokenBackendNotification& notification) override;

  void OnCanSendNetworkRequest(
      const OneTimeTokenBackendNotification& notification,
      base::TimeTicks trigger_time) override;

 private:
  void ProcessCachedNotifications();

  void RetrieveGmailOtp(const OneTimeTokenBackendNotification& notification,
                        base::TimeTicks trigger_time);

  void OnResponseFromGmailOtpBackend(
      const OneTimeTokenBackendNotification& notification,
      base::TimeTicks trigger_time,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  raw_ref<signin::IdentityManager> identity_manager_;

  // Handles subscriptions to the `GmailOtpBackend`.
  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;

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

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<GmailOtpBackendImpl> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
