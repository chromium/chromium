// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_SERVICE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
#include "url/origin.h"

namespace one_time_tokens {

class FakeGmailOtpService : public GmailOtpService {
 public:
  FakeGmailOtpService();
  FakeGmailOtpService(const FakeGmailOtpService&) = delete;
  FakeGmailOtpService& operator=(const FakeGmailOtpService&) = delete;
  ~FakeGmailOtpService() override;

  // GmailOtpService:
  GmailOtpBackend* GetBackend() override;
  void GetFreshestCachedOtp(const url::Origin& origin,
                            bool allow_psl_matching,
                            GetFreshestCachedOtpCallback callback) override;
  [[nodiscard]] ExpiringSubscription Subscribe(
      const url::Origin& origin,
      bool allow_psl_matching,
      base::Time expiration,
      IncomingTokenCallback callback,
      base::OnceClosure expiration_callback) override;
  bool HasPendingRequests(
      const ExpiringSubscription& subscription) const override;
  OneTimeTokenLogSink* GetLogSink() override;

  // Simulates reception of a token or error from backend.
  void ProcessCallbacks(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);
  void ProcessCallbacks(OneTimeToken token);

  void SetHasPendingRequests(bool has_pending_requests);
  void SetCachedTokens(std::vector<OneTimeToken> tokens);
  void ClearCache();

  size_t num_callbacks() const;
  size_t num_get_freshest_cached_otp_calls() const {
    return num_get_freshest_cached_otp_calls_;
  }
  size_t num_subscribe_calls() const { return num_subscribe_calls_; }

 private:
  void NotifySubscriber(
      const url::Origin& origin,
      bool allow_psl_matching,
      IncomingTokenCallback callback,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  static bool FrameMatchesSenderEmail(
      const url::Origin& frame_origin,
      bool allow_psl_matching,
      std::optional<std::string_view> sender_email);

  void DispatchCallbacks(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  FakeGmailOtpBackend backend_;
  std::vector<OneTimeToken> cached_tokens_;
  bool has_pending_requests_ = false;
  size_t num_get_freshest_cached_otp_calls_ = 0;
  size_t num_subscribe_calls_ = 0;
  ExpiringSubscriptionManager<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      subscription_manager_;

  base::WeakPtrFactory<FakeGmailOtpService> weak_ptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_SERVICE_H_
