// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_SMS_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_SMS_OTP_SERVICE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_cache.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace one_time_tokens {

// This implementation is for testing. Unlike `SmsOtpServiceImpl`, it is not
// backed by an `SmsOtpBackend`. Instead, it lets tests manually control and
// simulate the moment an SMS one-time password (OTP) is received, and inspect
// the subscriptions and requests that were made to the service.
class FakeSmsOtpService : public SmsOtpService {
 public:
  FakeSmsOtpService();
  FakeSmsOtpService(const FakeSmsOtpService&) = delete;
  FakeSmsOtpService& operator=(const FakeSmsOtpService&) = delete;
  ~FakeSmsOtpService() override;

  // SmsOtpService:
  void GetRecentOneTimeTokens(Callback callback) override;
  std::vector<OneTimeToken> GetCachedOneTimeTokens() const override;
  [[nodiscard]] ExpiringSubscription Subscribe(
      base::Time expiration,
      Callback callback,
      base::OnceClosure expiration_callback) override;
  void RequestOneTimeToken(
      base::TimeDelta timeout,
      base::OnceCallback<void(std::optional<OneTimeToken>)> callback) override;
  OneTimeTokenLogSink* log_sink() override;

  // Simulates the reception of an SMS OTP or of a retrieval error:
  // - A received token is added to the cache of recent tokens.
  // - All active subscribers are notified about `reply`.
  // - All pending `RequestOneTimeToken()` calls are completed with the received
  //   token, or with `std::nullopt` in case of an error.
  void SimulateOtpArrived(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  // Sets whether the platform supports SMS OTPs (true by default). If it does
  // not (e.g., on Desktop), the fake behaves like `SmsOtpServiceImpl` without
  // an `SmsOtpBackend`: `Subscribe()` immediately reports
  // `OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported` and
  // returns an inactive subscription, and `RequestOneTimeToken()` immediately
  // completes with `std::nullopt`.
  void set_platform_supported(bool platform_supported) {
    platform_supported_ = platform_supported;
  }

  // Returns the number of subscriptions that are neither expired nor
  // cancelled.
  size_t num_subscribers() const {
    return subscription_manager_.GetNumberSubscribers();
  }

  // Returns the number of `RequestOneTimeToken()` calls that have neither been
  // completed by `SimulateOtpArrived()` nor timed out.
  size_t num_pending_requests() const { return pending_requests_.size(); }

 private:
  using RequestCallback = base::OnceCallback<void(std::optional<OneTimeToken>)>;

  // Identifies duplicate tokens in `cache_`, analogously to
  // `SmsOtpServiceImpl`.
  struct CacheKey {
    OneTimeTokenType type;
    std::string_view value;
    std::optional<std::string_view> sender_address;
    bool operator==(const CacheKey&) const = default;
  };

  struct CacheProjection {
    CacheKey operator()(const OneTimeToken& token) const {
      return {token.type(), token.value(), token.sender_address()};
    }
  };

  // Completes the request identified by `request_id` with `std::nullopt` if it
  // is still pending.
  void OnRequestTimedOut(int request_id);

  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;

  // Recently received tokens.
  ExpiringCache<OneTimeToken,
                decltype(&OneTimeToken::on_device_arrival_time),
                CacheProjection>
      cache_;

  // Callbacks of pending `RequestOneTimeToken()` calls, keyed by request id.
  base::flat_map<int, RequestCallback> pending_requests_;
  int next_request_id_ = 0;

  bool platform_supported_ = true;

  OneTimeTokenLogSink log_sink_;

  base::WeakPtrFactory<FakeSmsOtpService> weak_ptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_SMS_OTP_SERVICE_H_
