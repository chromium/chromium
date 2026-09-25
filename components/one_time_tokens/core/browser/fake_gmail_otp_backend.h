// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_BACKEND_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace one_time_tokens {

// This implementation is for testing. It lets us manually control and simulate
// the moment a Gmail OTP is received for one-time passwords (OTP), and inspect
// received backend notifications.
class FakeGmailOtpBackend : public GmailOtpBackend {
 public:
  FakeGmailOtpBackend();
  ~FakeGmailOtpBackend() override;

  // GmailOtpBackend:
  ExpiringSubscription Subscribe(base::Time expiration,
                                 Callback callback) override;
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

  size_t num_callbacks() const { return callbacks_.size(); }

  const std::vector<OneTimeTokenBackendNotification>& incoming_notifications()
      const {
    return incoming_notifications_;
  }

 private:
  std::vector<Callback> callbacks_;
  std::vector<OneTimeTokenBackendNotification> incoming_notifications_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_BACKEND_H_
