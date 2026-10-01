// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_BACKEND_H_

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/one_time_tokens/core/browser/util/expiring_cache.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace one_time_tokens {

class OneTimeTokenLogSink;

// This implementation is for testing. It lets us manually control and simulate
// the moment a Gmail OTP is received for one-time passwords (OTP), and inspect
// received backend notifications.
class FakeGmailOtpBackend : public GmailOtpBackend {
 public:
  FakeGmailOtpBackend();
  ~FakeGmailOtpBackend() override;

  // GmailOtpBackend:
  void SetLogSink(OneTimeTokenLogSink* log_sink) override;
  OneTimeTokenLogSink* GetLogSink() const override;

  ExpiringSubscription Subscribe(base::Time expiration,
                                 Callback callback) override;
  ExpiringSubscription Subscribe(
      base::Time expiration,
      Callback callback,
      base::OnceClosure expiration_callback) override;
  ExpiringSubscription SubscribeToTickles(base::Time expiration,
                                          TickleCallback callback) override;
  std::vector<OneTimeToken> GetCachedOneTimeTokens() const override;
  std::vector<OneTimeToken> PurgeExpiredAndGetCachedOneTimeTokens() override;
  void OnIncomingOneTimeTokenBackendNotification(
      const OneTimeTokenBackendNotification& notification) override;
  void FetchUserDataProcessingConsent(
      FetchUserDataProcessingConsentCallback callback) override;
  bool HasPendingRequests() const override;

  // Simulates the reception of a Gmail OTP.
  void ProcessCallbacks(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  void SetHasPendingRequests(bool has_pending_requests);
  void SetUserDataProcessingConsentStates(
      std::optional<UserDataProcessingConsentStates> states);
  void ClearCache();

  size_t num_callbacks() const {
    return subscription_manager_.GetNumberSubscribers();
  }

  void SetUserDataProcessingConsent(
      std::optional<UserDataProcessingConsentStates> consent_states) {
    consent_states_ = std::move(consent_states);
  }

  const std::vector<OneTimeTokenBackendNotification>& incoming_notifications()
      const {
    return incoming_notifications_;
  }

 private:
  struct OneTimeTokenCacheKey {
    std::string value;
    std::optional<std::string> sender_address;
    bool operator==(const OneTimeTokenCacheKey&) const = default;
  };

  struct OneTimeTokenCacheProjection {
    OneTimeTokenCacheKey operator()(const OneTimeToken& token) const {
      return {token.value(), token.sender_address()};
    }
  };

  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;
  ExpiringSubscriptionManager<void()> tickle_subscription_manager_;
  ExpiringCache<OneTimeToken,
                decltype(&OneTimeToken::on_device_arrival_time),
                OneTimeTokenCacheProjection>
      one_time_token_cache_;
  std::vector<OneTimeTokenBackendNotification> incoming_notifications_;
  std::optional<UserDataProcessingConsentStates> consent_states_;
  std::optional<OneTimeTokenRetrievalError> pending_error_;
  raw_ptr<OneTimeTokenLogSink> log_sink_ = nullptr;
  bool has_pending_requests_ = false;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_BACKEND_H_
