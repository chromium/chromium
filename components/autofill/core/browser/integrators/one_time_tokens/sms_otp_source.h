// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_SMS_OTP_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_SMS_OTP_SOURCE_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace one_time_tokens {
class SmsOtpService;
}  // namespace one_time_tokens

namespace url {
class Origin;
}  // namespace url

namespace autofill {

// SMS OTP source owned by `OtpCoordinator`. Queries recent SMS OTPs and
// maintains a subscription with `SmsOtpService` while OTP fields are present.
class SmsOtpSource : public OtpSource {
 public:
  // `sms_otp_service` may be null (e.g. on platforms without an SMS backend),
  // in which case `Start()` is a no-op. If non-null, `SmsOtpSource` retains an
  // unowned pointer and `*sms_otp_service` must outlive `this`.
  explicit SmsOtpSource(one_time_tokens::SmsOtpService* sms_otp_service);
  SmsOtpSource(const SmsOtpSource&) = delete;
  SmsOtpSource& operator=(const SmsOtpSource&) = delete;
  ~SmsOtpSource() override;

  // OtpSource:
  void Start(const OtpTarget& target) override;
  void Stop() override;
  std::vector<OtpCandidate> GetAvailable(
      const OtpTarget& target) const override;
  void MarkConsumed(const OtpTokenIdentity& identity) override;
  void ClearVerifiedCacheForOrigin(const url::Origin& origin) override;
  void ClearAllVerifiedCache() override;
  // `observer` may be null to unregister; if non-null, `SmsOtpSource` retains
  // an unowned pointer and `*observer` must outlive `this` or unregister via
  // `SetObserver(nullptr)` before destruction.
  void SetObserver(Observer* observer) override;

 private:
  void OnOneTimeTokenReceived(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>
          token_or_error);
  void OnSubscriptionExpired();
  bool HasUnexpiredNewestToken(base::TimeTicks now) const;

  const raw_ptr<one_time_tokens::SmsOtpService> sms_otp_service_;
  raw_ptr<Observer> observer_ = nullptr;
  one_time_tokens::ExpiringSubscription subscription_;
  // Newest SMS OTP seen so far (whether consumed or not), used as a monotonic
  // floor so older or duplicate tokens are never offered. Because Android
  // re-polls the SMS backend every 5s and stamps each reply with a fresh
  // arrival time, a same-value delivery while `newest_sms_token_` is unexpired
  // (`kCacheDurationForOldTokens`, 3 minutes) is ignored while retaining the
  // initial arrival time; as a trade-off, a genuine resend of the exact same
  // code within 3 minutes is ignored.
  std::optional<one_time_tokens::OneTimeToken> newest_sms_token_;
  bool is_newest_sms_token_consumed_ = false;

  base::WeakPtrFactory<SmsOtpSource> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_SMS_OTP_SOURCE_H_
