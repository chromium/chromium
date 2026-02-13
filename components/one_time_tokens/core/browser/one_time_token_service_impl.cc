// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"

#include "base/containers/adapters.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

namespace one_time_tokens {

OneTimeTokenServiceImpl::OneTimeTokenServiceImpl(
    SmsOtpBackend* sms_otp_backend,
    GmailOtpBackend* gmail_otp_backend)
    : sms_{.has_pending_request = false, .backend = sms_otp_backend},
      gmail_{.has_pending_request = false, .backend = gmail_otp_backend},
      cache_(kCacheDurationForOldTokens) {}
OneTimeTokenServiceImpl::~OneTimeTokenServiceImpl() = default;

void OneTimeTokenServiceImpl::GetRecentOneTimeTokens(Callback callback) {
  std::vector<OneTimeToken> recent_tokens =
      base::ToVector(cache_.PurgeExpiredAndGetCache(),
                     [](const OneTimeToken& token) { return token; });
  // The tokens in `cache_` are sorted by `on_device_arrival_time` in ascending
  // order. We want to deliver the most recent token first, but `callback` does
  // not strictly guarantee this order, because multiple backends may
  // need to be asked in parallel in the future.
  for (const auto& token : base::Reversed(recent_tokens)) {
    OneTimeTokenSource source;
    switch (token.type()) {
      case OneTimeTokenType::kSmsOtp:
        source = OneTimeTokenSource::kOnDeviceSms;
        break;
      case OneTimeTokenType::kGmail:
        source = OneTimeTokenSource::kGmail;
        break;
    }
    callback.Run(source, base::ok(token));
  }
}

ExpiringSubscription OneTimeTokenServiceImpl::Subscribe(base::Time expiration,
                                                        Callback callback) {
  ExpiringSubscription subscription =
      subscription_manager_.Subscribe(expiration, std::move(callback));
  RetrieveSmsOtpIfNeeded();
  RetrieveGmailOtpIfNeeded();
  return subscription;
}

std::vector<OneTimeToken> OneTimeTokenServiceImpl::GetCachedOneTimeTokens()
    const {
  return base::ToVector(cache_.GetCache(),
                        [](const OneTimeToken& token) { return token; });
}

void OneTimeTokenServiceImpl::RequestOneTimeToken(
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
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  sms_.has_pending_request = false;
  if (!reply.has_value()) {
    // TODO(crbug.com/415273270) Do proper error handling:
    // - In case of timeout, schedule a refetch if appropriate.
    // - In case of a permission error or API error, report the problems.
    subscription_manager_.Notify(OneTimeTokenSource::kOnDeviceSms,
                                 base::unexpected(reply.error()));
    return;
  }

  const OneTimeToken& token = reply.value();
  cache_.PurgeExpiredAndAdd(token);
  // Instead of notifying subscribers only if the OTP is actually new,
  // subscribers are always notified. This ensures that newly added subscribers
  // who missed notifications from before their subscription are informed.
  subscription_manager_.Notify(OneTimeTokenSource::kOnDeviceSms,
                               base::ok(token));

  // It's possible that the SMS OTP backend responded with a stale OTP.
  // Therefore, schedule a new retrieval to see if a new OTP arrives.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OneTimeTokenServiceImpl::RetrieveSmsOtpIfNeeded,
                     weakptr_factory_.GetWeakPtr()),
      kSmsRefetchInterval);
}

void OneTimeTokenServiceImpl::RetrieveGmailOtpIfNeeded() {
  if (!gmail_.backend || gmail_.has_pending_request ||
      !subscription_manager_.GetNumberSubscribers()) {
    return;
  }
  gmail_subscription_ = gmail_.backend->Subscribe(
      base::Time::Now() + kCacheDurationForOldTokens,
      base::BindRepeating(
          &OneTimeTokenServiceImpl::OnResponseFromGmailOtpBackend,
          weakptr_factory_.GetWeakPtr()));

  gmail_.has_pending_request = true;
}

void OneTimeTokenServiceImpl::OnResponseFromGmailOtpBackend(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  if (reply.has_value()) {
    cache_.PurgeExpiredAndAdd(*reply);
  }
  subscription_manager_.Notify(OneTimeTokenSource::kGmail, std::move(reply));
}

}  // namespace one_time_tokens
