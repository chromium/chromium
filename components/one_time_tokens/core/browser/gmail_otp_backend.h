// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

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

// Duration after which tokens expire in the internal token cache.
inline constexpr base::TimeDelta kGmailTokenCacheDuration = base::Minutes(1);

// Abstract interface for fetching OTPs from Gmail.
class GmailOtpBackend : public KeyedService {
 public:
  using CallbackSignature =
      void(base::expected<OneTimeToken, OneTimeTokenRetrievalError>);
  using Callback = base::RepeatingCallback<CallbackSignature>;
  using FetchUserDataProcessingConsentCallback =
      base::OnceCallback<void(std::optional<UserDataProcessingConsentStates>)>;
  using TickleCallback = base::RepeatingClosure;

  ~GmailOtpBackend() override;

  // Creates a new instance of the backend.
  static std::unique_ptr<GmailOtpBackend> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager);

  virtual void SetLogSink(OneTimeTokenLogSink* log_sink) = 0;
  virtual OneTimeTokenLogSink* GetLogSink() const = 0;

  // Creates a subscription for new incoming OTPs.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(base::Time expiration,
                                                       Callback callback) = 0;

  // Creates a subscription for new incoming OTPs with an expiration callback.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(
      base::Time expiration,
      Callback callback,
      base::OnceClosure expiration_callback) = 0;

  // Creates a subscription for incoming push notifications (tickles) without
  // triggering token fetching or network requests.
  [[nodiscard]] virtual ExpiringSubscription SubscribeToTickles(
      base::Time expiration,
      TickleCallback callback) = 0;

  // Returns all cached one-time tokens, without filtering for expiration.
  virtual std::vector<OneTimeToken> GetCachedOneTimeTokens() const = 0;

  // Purges expired tokens and returns the remaining cached one-time tokens.
  virtual std::vector<OneTimeToken> PurgeExpiredAndGetCachedOneTimeTokens() = 0;

  // Called when a new OTP is received via the OneTimeToken notification.
  virtual void OnIncomingOneTimeTokenBackendNotification(
      const OneTimeTokenBackendNotification& notification) = 0;

  // Returns true if there are cached notifications or any in-flight requests.
  virtual bool HasPendingRequests() const = 0;

  // Fetches the user data processing consent states from the backend.
  virtual void FetchUserDataProcessingConsent(
      FetchUserDataProcessingConsentCallback callback) = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
