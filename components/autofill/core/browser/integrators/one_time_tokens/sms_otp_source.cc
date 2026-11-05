// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/sms_otp_source.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "url/origin.h"

namespace autofill {
namespace {

bool IsTokenExpired(const one_time_tokens::OneTimeToken& token,
                    base::TimeTicks now) {
  const base::TimeTicks arrival = token.on_device_arrival_time();
  return arrival.is_null() ||
         now - arrival > one_time_tokens::kCacheDurationForOldTokens;
}

}  // namespace

SmsOtpSource::SmsOtpSource(one_time_tokens::SmsOtpService* sms_otp_service)
    : sms_otp_service_(sms_otp_service) {}

SmsOtpSource::~SmsOtpSource() = default;

void SmsOtpSource::Start(const OtpTarget& target) {
  if (!sms_otp_service_) {
    return;
  }

  static constexpr base::TimeDelta kSmsOtpSubscriptionDuration =
      base::Minutes(1);
  const base::Time expiration = base::Time::Now() + kSmsOtpSubscriptionDuration;
  base::WeakPtr<SmsOtpSource> weak_this = weak_ptr_factory_.GetWeakPtr();
  if (subscription_.IsAlive()) {
    subscription_.SetExpirationTime(expiration);
  } else {
    // `SmsOtpServiceImpl::Subscribe()` can invoke `OnOneTimeTokenReceived()`
    // -> `observer_->OnSourceCandidateAvailable()` synchronously before
    // returning (e.g. when the backend is missing or bridge initialization
    // failed). Guard against the observer re-entrantly calling `Stop()` (which
    // invalidates `weak_this`), `Start()` (which populates `subscription_`),
    // or destroying `this`.
    // TODO(crbug.com/581795804, crbug.com/581805706): Remove these synchronous
    // callback re-entrancy guards once `SmsOtpService::Subscribe()` invokes its
    // callback strictly asynchronously.
    one_time_tokens::ExpiringSubscription subscription =
        sms_otp_service_->Subscribe(
            expiration,
            base::BindRepeating(&SmsOtpSource::OnOneTimeTokenReceived,
                                weak_this),
            base::BindOnce(&SmsOtpSource::OnSubscriptionExpired, weak_this));
    if (!weak_this) {
      return;
    }
    if (!subscription_.IsAlive()) {
      subscription_ = std::move(subscription);
    }
  }

  // `GetRecentOneTimeTokens()` invokes its callback synchronously, which can
  // immediately deliver cached OTPs to `observer_`. Keep this call last after
  // `subscription_` is updated.
  sms_otp_service_->GetRecentOneTimeTokens(
      base::BindRepeating(&SmsOtpSource::OnOneTimeTokenReceived, weak_this));
}

void SmsOtpSource::Stop() {
  subscription_ = {};
  weak_ptr_factory_.InvalidateWeakPtrs();
}

std::vector<OtpCandidate> SmsOtpSource::GetAvailable(
    const OtpTarget& target) const {
  if (!HasUnexpiredNewestToken(base::TimeTicks::Now()) ||
      is_newest_sms_token_consumed_) {
    return {};
  }
  // SMS OTPs are origin-agnostic at the source layer (`SmsOtpSource` does not
  // verify `target.origin` or `target.frame`); `OtpCoordinator` binds and
  // validates the target origin and frame before delivery.
  // TODO(crbug.com/581805923): Store `verified_origin` and `verified_frame` as
  // `std::optional` in `OtpCandidate` instead of passing empty values.
  return {OtpCandidate(*newest_sms_token_, url::Origin(), LocalFrameToken())};
}

void SmsOtpSource::MarkConsumed(const OtpTokenIdentity& identity) {
  if (newest_sms_token_.has_value() &&
      newest_sms_token_->value() == identity.value) {
    is_newest_sms_token_consumed_ = true;
  }
}

void SmsOtpSource::ClearVerifiedCacheForOrigin(const url::Origin& origin) {
  // Intentional no-op: SMS OTPs are origin-agnostic and are not cached per
  // origin.
}

void SmsOtpSource::ClearAllVerifiedCache() {
  // Intentional no-op: `newest_sms_token_` and `is_newest_sms_token_consumed_`
  // act as a monotonic deduplication and consumption floor across
  // `SmsOtpService` re-polls/replays. Clearing them on form removal or
  // navigation would allow `SmsOtpService` to re-emit an already-consumed or
  // outdated SMS token on the next `Start()`.
}

void SmsOtpSource::SetObserver(Observer* observer) {
  observer_ = observer;
}

void SmsOtpSource::OnOneTimeTokenReceived(
    base::expected<one_time_tokens::OneTimeToken,
                   one_time_tokens::OneTimeTokenRetrievalError>
        token_or_error) {
  if (!token_or_error.has_value()) {
    // `SmsOtpServiceImpl` does not schedule a retry after a backend error, so
    // drop `subscription_` (and invalidate `weak_this` if invoked synchronously
    // inside `Subscribe()`) so the next `Start()` subscribes again and triggers
    // a fresh fetch.
    subscription_ = {};
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (observer_) {
      observer_->OnSourceCandidateAvailable(
          /*target=*/std::nullopt, base::unexpected(token_or_error.error()));
    }
    return;
  }

  const one_time_tokens::OneTimeToken& token = *token_or_error;
  // `SmsOtpSource` only subscribes to `SmsOtpService`, which exclusively
  // produces `kSmsOtp` tokens (Gmail OTPs are handled separately by
  // `GmailOtpSource` via `GmailOtpService`).
  CHECK_EQ(token.type(), one_time_tokens::OneTimeTokenType::kSmsOtp);
  const base::TimeTicks now = base::TimeTicks::Now();
  if (token.value().empty() || IsTokenExpired(token, now)) {
    return;
  }

  if (HasUnexpiredNewestToken(now)) {
    if (token.value() == newest_sms_token_->value() ||
        token.on_device_arrival_time() <=
            newest_sms_token_->on_device_arrival_time()) {
      return;
    }
  }

  newest_sms_token_ = token;
  is_newest_sms_token_consumed_ = false;
  if (observer_) {
    // TODO(crbug.com/581805923): Store `verified_origin` and `verified_frame`
    // as `std::optional` in `OtpCandidate` instead of passing empty values.
    observer_->OnSourceCandidateAvailable(
        /*target=*/std::nullopt,
        OtpCandidate(token, url::Origin(), LocalFrameToken()));
  }
}

void SmsOtpSource::OnSubscriptionExpired() {
  if (observer_) {
    observer_->OnSourceCandidateAvailable(
        /*target=*/std::nullopt,
        base::unexpected(
            one_time_tokens::OneTimeTokenRetrievalError::kSubscriptionExpired));
  }
}

bool SmsOtpSource::HasUnexpiredNewestToken(base::TimeTicks now) const {
  return newest_sms_token_.has_value() &&
         !IsTokenExpired(*newest_sms_token_, now);
}

}  // namespace autofill
