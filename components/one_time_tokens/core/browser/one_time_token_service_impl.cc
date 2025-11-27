// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"

#include <variant>

#include "base/containers/adapters.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace one_time_tokens {

OneTimeTokenServiceImpl::OneTimeTokenServiceImpl(SmsOtpBackend* sms_otp_backend)
    : sms_{.has_pending_request = false, .backend = sms_otp_backend},
      cache_(kCacheDurationForOldTokens) {}
OneTimeTokenServiceImpl::~OneTimeTokenServiceImpl() = default;

void OneTimeTokenServiceImpl::GetRecentOneTimeTokens(Callback callback) {
  const std::list<OneTimeToken>& recent_tokens =
      cache_.PurgeExpiredAndGetCache();
  // Tokens are returned in most recent first order. This allows the receiver to
  // ignore older tokens if newer ones have been received before. We can't
  // strictly guarantee this order, though, because multiple backends may need
  // to be asked in parallel in the future.
  for (const auto& token : base::Reversed(recent_tokens)) {
    callback.Run(OneTimeTokenSource::kOnDeviceSms, base::ok(token));
  }
}

std::vector<OneTimeToken> OneTimeTokenServiceImpl::GetCachedOneTimeTokens()
    const {
  return base::ToVector(cache_.GetCache());
}

ExpiringSubscription OneTimeTokenServiceImpl::Subscribe(base::Time expiration,
                                                        Callback callback) {
  ExpiringSubscription subscription =
      subscription_manager_.Subscribe(expiration, std::move(callback));
  RetrieveSmsOtpIfNeeded();
  return subscription;
}

void OneTimeTokenServiceImpl::RetrieveSmsOtpIfNeeded() {
  if (!sms_.backend || sms_.has_pending_request ||
      !subscription_manager_.GetNumberSubscribers()) {
    return;
  }
  sms_.backend->RetrieveSmsOtp(
      base::BindOnce(&OneTimeTokenServiceImpl::OnResponseFromSmsOtpBackend,
                     weakptr_factory_.GetWeakPtr()));

  sms_.has_pending_request = true;
}

void OneTimeTokenServiceImpl::OnResponseFromSmsOtpBackend(
    const OtpFetchReply& reply) {
  sms_.has_pending_request = false;
  if (!reply.request_complete || !reply.otp_value.has_value()) {
    // TODO(crbug.com/415273270) Do proper error handling:
    // - In case of timeout, schedule a refetch if appropriate.
    // - In case of a permission error or API error, report the problems.
    subscription_manager_.Notify(
        OneTimeTokenSource::kOnDeviceSms,
        base::unexpected(OneTimeTokenRetrievalError::kUnknown));
    return;
  }

  cache_.PurgeExpiredAndAdd(*reply.otp_value);
  // Instead of notifying subscribers only if the OTP is actually new,
  // subscribers are always notified. This ensures that newly added subscribers
  // who missed notifications from before their subscription are informed.
  subscription_manager_.Notify(OneTimeTokenSource::kOnDeviceSms,
                               base::ok(*reply.otp_value));

  // It's possible that the SMS OTP backend responded with a stale OTP.
  // Therefore, schedule a new retrieval to see if a new OTP arrives.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OneTimeTokenServiceImpl::RetrieveSmsOtpIfNeeded,
                     weakptr_factory_.GetWeakPtr()),
      kSmsRefetchInterval);
}

}  // namespace one_time_tokens
