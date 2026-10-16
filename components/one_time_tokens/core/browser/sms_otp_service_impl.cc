// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/sms_otp_service_impl.h"

#include <ranges>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"

namespace one_time_tokens {

SmsOtpServiceImpl::SmsOtpServiceImpl(
    SmsOtpBackend* sms_otp_backend,
    OneTimeTokenLogSink::LogHandler log_handler)
    : sms_{.has_pending_request = false, .backend = sms_otp_backend},
      cache_(kCacheDurationForOldTokens,
             &OneTimeToken::on_device_arrival_time,
             CacheProjection()) {
  if (log_handler) {
    log_subscription_ = log_sink_.AddLogHandler(std::move(log_handler));
  }
}

SmsOtpServiceImpl::~SmsOtpServiceImpl() = default;

OneTimeTokenLogSink* SmsOtpServiceImpl::log_sink() {
  return &log_sink_;
}

void SmsOtpServiceImpl::GetRecentOneTimeTokens(Callback callback) {
  // The tokens in `cache_` are sorted by `on_device_arrival_time` in ascending
  // order. Iterate in reverse so that the most recent token is delivered first.
  std::vector<OneTimeToken> recent_tokens =
      base::ToVector(cache_.PurgeExpiredAndGetItems());
  for (const auto& token : std::views::reverse(recent_tokens)) {
    callback.Run(base::ok(token));
  }
}

ExpiringSubscription SmsOtpServiceImpl::Subscribe(
    base::Time expiration,
    Callback callback,
    base::OnceClosure expiration_callback) {
  if (!sms_.backend) {
    callback.Run(base::unexpected(
        OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported));
    return ExpiringSubscription();
  }
  ExpiringSubscription subscription = sms_subscription_manager_.Subscribe(
      expiration, std::move(callback), std::move(expiration_callback));
  RetrieveSmsOtpIfNeeded();
  return subscription;
}

std::vector<OneTimeToken> SmsOtpServiceImpl::GetCachedOneTimeTokens() const {
  return base::ToVector(cache_.GetItems());
}

void SmsOtpServiceImpl::RequestOneTimeToken(
    base::TimeDelta timeout,
    base::OnceCallback<void(std::optional<OneTimeToken>)> callback) {
  auto on_request_finished = base::BindRepeating(
      [](base::OnceCallback<void(std::optional<OneTimeToken>)>& callback,
         std::optional<OneTimeToken> token) {
        if (callback) {
          std::move(callback).Run(std::move(token));
        }
      },
      base::OwnedRef(std::move(callback)));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(on_request_finished, std::nullopt), timeout);

  if (sms_.backend) {
    sms_.backend->RetrieveSmsOtp(base::BindOnce(
        [](base::RepeatingCallback<void(std::optional<OneTimeToken>)> callback,
           base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
          callback.Run(base::OptionalFromExpected(std::move(result)));
        },
        on_request_finished));
  } else {
    on_request_finished.Run(std::nullopt);
  }
}

void SmsOtpServiceImpl::RetrieveSmsOtpIfNeeded() {
  if (sms_.has_pending_request ||
      !sms_subscription_manager_.GetNumberSubscribers()) {
    return;
  }
  CHECK(sms_.backend);
  sms_.has_pending_request = true;
  sms_.backend->RetrieveSmsOtp(
      base::BindOnce(&SmsOtpServiceImpl::OnResponseFromSmsOtpBackend,
                     weakptr_factory_.GetWeakPtr()));
}

void SmsOtpServiceImpl::OnResponseFromSmsOtpBackend(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  sms_.has_pending_request = false;
  if (!reply.has_value()) {
    // TODO(crbug.com/415273270) Do proper error handling:
    // - In case of timeout, schedule a refetch if appropriate.
    // - In case of a permission error or API error, report the problems.
    sms_subscription_manager_.Notify(base::unexpected(reply.error()));
    return;
  }

  const OneTimeToken& token = reply.value();
  cache_.PurgeExpiredAndAdd(token);
  // Instead of notifying subscribers only if the OTP is actually new,
  // subscribers are always notified. This ensures that newly added subscribers
  // who missed notifications from before their subscription are informed.
  sms_subscription_manager_.Notify(base::ok(token));

  // It's possible that the SMS OTP backend responded with a stale OTP.
  // Therefore, schedule a new retrieval to see if a new OTP arrives.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SmsOtpServiceImpl::RetrieveSmsOtpIfNeeded,
                     weakptr_factory_.GetWeakPtr()),
      kSmsRefetchInterval);
}

}  // namespace one_time_tokens
