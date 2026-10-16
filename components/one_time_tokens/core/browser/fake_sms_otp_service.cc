// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"

#include <ranges>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace one_time_tokens {

FakeSmsOtpService::FakeSmsOtpService()
    : cache_(kCacheDurationForOldTokens,
             &OneTimeToken::on_device_arrival_time,
             CacheProjection()) {}

FakeSmsOtpService::~FakeSmsOtpService() = default;

void FakeSmsOtpService::GetRecentOneTimeTokens(Callback callback) {
  // Like `SmsOtpServiceImpl`, deliver the most recent token first.
  std::vector<OneTimeToken> recent_tokens =
      base::ToVector(cache_.PurgeExpiredAndGetItems());
  for (const OneTimeToken& token : std::views::reverse(recent_tokens)) {
    callback.Run(base::ok(token));
  }
}

std::vector<OneTimeToken> FakeSmsOtpService::GetCachedOneTimeTokens() const {
  return base::ToVector(cache_.GetItems());
}

ExpiringSubscription FakeSmsOtpService::Subscribe(
    base::Time expiration,
    Callback callback,
    base::OnceClosure expiration_callback) {
  if (!platform_supported_) {
    callback.Run(base::unexpected(
        OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported));
    return ExpiringSubscription();
  }
  return subscription_manager_.Subscribe(expiration, std::move(callback),
                                         std::move(expiration_callback));
}

void FakeSmsOtpService::RequestOneTimeToken(base::TimeDelta timeout,
                                            RequestCallback callback) {
  if (!platform_supported_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  const int request_id = next_request_id_++;
  pending_requests_.emplace(request_id, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeSmsOtpService::OnRequestTimedOut,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      timeout);
}

OneTimeTokenLogSink* FakeSmsOtpService::log_sink() {
  return &log_sink_;
}

void FakeSmsOtpService::SimulateOtpArrived(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  if (reply.has_value()) {
    cache_.PurgeExpiredAndAdd(reply.value());
  }
  subscription_manager_.Notify(reply);

  // Callbacks may issue new requests, so complete only the ones that are
  // pending at this point.
  std::optional<OneTimeToken> token =
      reply.has_value() ? std::make_optional(reply.value()) : std::nullopt;
  for (auto& request : std::exchange(pending_requests_, {})) {
    std::move(request.second).Run(token);
  }
}

void FakeSmsOtpService::OnRequestTimedOut(int request_id) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    return;
  }
  RequestCallback callback = std::move(it->second);
  pending_requests_.erase(it);
  std::move(callback).Run(std::nullopt);
}

}  // namespace one_time_tokens
