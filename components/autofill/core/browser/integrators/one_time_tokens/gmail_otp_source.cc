// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_source.h"

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "components/prefs/pref_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {
namespace {

bool IsTokenExpired(const one_time_tokens::OneTimeToken& token,
                    base::TimeTicks now) {
  return token.on_device_arrival_time().is_null() ||
         now - token.on_device_arrival_time() > kGmailOtpCacheDuration;
}

base::Time ComputeSubscriptionExpiration() {
  base::TimeDelta period =
      one_time_tokens::features::kGmailOtpSubscriptionPeriodParam.Get();
  if (!period.is_positive()) {
    period = one_time_tokens::features::kGmailOtpSubscriptionPeriodParam
                 .default_value;
  }
  return base::Time::Now() + period;
}

ukm::SourceId GetPageUkmSourceId(AutofillClient& client) {
  for (AutofillDriver* driver :
       client.GetAutofillDriverFactory().GetExistingDrivers()) {
    if (driver && !driver->GetParent() && IsActiveNonEmbeddedDriver(*driver)) {
      return driver->GetPageUkmSourceId();
    }
  }
  return ukm::kInvalidSourceId;
}

}  // namespace

bool GmailOtpSource::OrderByOnDeviceArrival::operator()(
    const OtpTokenIdentity& lhs,
    const OtpTokenIdentity& rhs) const {
  return std::tie(lhs.on_device_arrival, lhs.value, lhs.sender,
                  lhs.email_received_timestamp) <
         std::tie(rhs.on_device_arrival, rhs.value, rhs.sender,
                  rhs.email_received_timestamp);
}

bool GmailOtpSource::OrderByOnDeviceArrival::operator()(
    const OtpTokenIdentity& lhs,
    base::TimeTicks rhs_arrival) const {
  return lhs.on_device_arrival < rhs_arrival;
}

bool GmailOtpSource::OrderByOnDeviceArrival::operator()(
    base::TimeTicks lhs_arrival,
    const OtpTokenIdentity& rhs) const {
  return lhs_arrival < rhs.on_device_arrival;
}

GmailOtpSource::GmailOtpSource(AutofillClient& client)
    : client_(client), gmail_otp_service_(client.GetGmailOtpService()) {
  if (!gmail_otp_service_) {
    return;
  }
  // TODO(crbug.com/415273270): Observe smart-features consent changes once
  // exposed.
  if (PrefService* prefs = client_->GetPrefs()) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kAutofillGmailOtpFillingEnabled,
        base::BindRepeating(
            &GmailOtpSource::OnGmailOtpFillingEnabledPrefChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

GmailOtpSource::~GmailOtpSource() = default;

void GmailOtpSource::Start(const OtpTarget& target) {
  if (!gmail_otp_service_ || target.origin.opaque()) {
    return;
  }
  continuous_subscriptions_per_origin_.try_emplace(target.origin);
  EnsureContinuousSubscriptionForOrigin(target.origin);
  FetchFreshestCachedOtpForOrigin(target.origin);
}

void GmailOtpSource::Stop() {
  continuous_subscriptions_per_origin_.clear();
  // `GmailOtpService` owns the profile-scoped backend cache and replays the
  // freshest matching OTP via `GetFreshestCachedOtp()` on the next `Start()`,
  // so the tab-local `verified_store_per_origin_` can be cleared here while
  // retaining unexpired `consumed_tokens_` to suppress replays of already-used
  // OTPs.
  verified_store_per_origin_.clear();
  PruneExpiredConsumedTokens(base::TimeTicks::Now());
}

void GmailOtpSource::StopListeningForOrigin(const url::Origin& origin) {
  if (origin.opaque()) {
    return;
  }
  continuous_subscriptions_per_origin_.erase(origin);
  verified_store_per_origin_.erase(origin);
}

std::vector<OtpCandidate> GmailOtpSource::GetAvailable(
    const OtpTarget& target) const {
  const base::TimeTicks now = base::TimeTicks::Now();
  if (!UserOptedIntoGmailOtpFilling() || target.origin.opaque()) {
    return {};
  }
  const one_time_tokens::OneTimeToken* stored_token =
      base::FindOrNull(verified_store_per_origin_, target.origin);
  if (!stored_token || stored_token->value().empty() ||
      IsTokenExpired(*stored_token, now)) {
    return {};
  }
  CHECK_EQ(stored_token->type(), one_time_tokens::OneTimeTokenType::kGmail);
  OtpCandidate candidate(*stored_token, target.origin, target.frame);
  if (consumed_tokens_.contains(candidate.identity)) {
    return {};
  }
  std::vector<OtpCandidate> candidates;
  candidates.reserve(1);
  candidates.push_back(std::move(candidate));
  return candidates;
}

void GmailOtpSource::MarkConsumed(const OtpTokenIdentity& identity) {
  // Only Gmail OTP identities (which always populate `sender`) can match
  // candidates produced by `GmailOtpSource`.
  if (identity.value.empty() || !identity.sender.has_value()) {
    return;
  }
  consumed_tokens_.insert(identity);
  PruneExpiredConsumedTokens(base::TimeTicks::Now());
}

void GmailOtpSource::ClearVerifiedCacheForOrigin(const url::Origin& origin) {
  if (origin.opaque()) {
    return;
  }
  verified_store_per_origin_.erase(origin);
}

void GmailOtpSource::ClearAllVerifiedCache() {
  verified_store_per_origin_.clear();
}

void GmailOtpSource::SetObserver(Observer* observer) {
  observer_ = observer;
}

bool GmailOtpSource::HasActiveSubscriptionForOriginForTesting(
    const url::Origin& origin) const {
  return HasActiveSubscriptionForOrigin(origin);
}

void GmailOtpSource::EnsureContinuousSubscriptionForOrigin(
    const url::Origin& origin) {
  CHECK(gmail_otp_service_);
  one_time_tokens::ExpiringSubscription* subscription =
      base::FindOrNull(continuous_subscriptions_per_origin_, origin);
  if (!subscription || !UserOptedIntoGmailOtpFilling()) {
    return;
  }
  const base::Time expiration = ComputeSubscriptionExpiration();
  if (subscription->IsAlive()) {
    subscription->SetExpirationTime(expiration);
    return;
  }
  *subscription = SubscribeToService(origin, expiration);
}

void GmailOtpSource::FetchFreshestCachedOtpForOrigin(
    const url::Origin& origin) {
  CHECK(gmail_otp_service_);
  if (!HasActiveSubscriptionForOrigin(origin)) {
    return;
  }
  gmail_otp_service_->GetFreshestCachedOtp(
      origin, GetPageUkmSourceId(*client_), /*allow_psl_matching=*/true,
      base::BindOnce(&GmailOtpSource::OnCachedOtpRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), origin));
}

one_time_tokens::ExpiringSubscription GmailOtpSource::SubscribeToService(
    const url::Origin& origin,
    base::Time expiration) {
  CHECK(gmail_otp_service_);
  return gmail_otp_service_->Subscribe(
      origin, GetPageUkmSourceId(*client_), /*allow_psl_matching=*/true,
      expiration,
      base::BindRepeating(&GmailOtpSource::OnIncomingTokenReceived,
                          weak_ptr_factory_.GetWeakPtr(), origin),
      base::BindOnce(&GmailOtpSource::EnsureContinuousSubscriptionForOrigin,
                     weak_ptr_factory_.GetWeakPtr(), origin));
}

void GmailOtpSource::OnGmailOtpFillingEnabledPrefChanged() {
  if (!UserOptedIntoGmailOtpFilling()) {
    for (auto& [origin, subscription] : continuous_subscriptions_per_origin_) {
      subscription.Cancel();
    }
    verified_store_per_origin_.clear();
    return;
  }
  for (const auto& [origin, _] : continuous_subscriptions_per_origin_) {
    EnsureContinuousSubscriptionForOrigin(origin);
    FetchFreshestCachedOtpForOrigin(origin);
  }
}

void GmailOtpSource::OnCachedOtpRetrieved(
    const url::Origin& origin,
    std::optional<one_time_tokens::OneTimeToken> token) {
  // A cached token replayed from `GmailOtpService` after `Stop()` + `Start()`
  // may already have been consumed earlier in this tab.
  if (!token ||
      consumed_tokens_.contains(OtpTokenIdentity::FromOneTimeToken(*token))) {
    return;
  }
  OnIncomingTokenReceived(origin, *std::move(token));
}

void GmailOtpSource::OnIncomingTokenReceived(
    const url::Origin& origin,
    base::expected<one_time_tokens::OneTimeToken,
                   one_time_tokens::OneTimeTokenRetrievalError> result) {
  if (!HasActiveSubscriptionForOrigin(origin)) {
    return;
  }

  if (!result.has_value()) {
    // `GmailOtpService` notifies every active subscriber whenever any Gmail OTP
    // arrives and reports `kGmailOtpSenderDomainMismatch` to subscribers whose
    // origin did not match that email's sender domain. Keep listening silently
    // for matching emails.
    if (result.error() == one_time_tokens::OneTimeTokenRetrievalError::
                              kGmailOtpSenderDomainMismatch) {
      return;
    }
    LOG_AF(client_->GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP retrieval resulted in a failure: "
        << base::ToString(result.error());
    if (observer_) {
      observer_->OnSourceCandidateAvailable(/*target=*/std::nullopt,
                                            base::unexpected(result.error()));
    }
    return;
  }

  if (ProcessMatchedToken(origin, *result) && observer_) {
    observer_->OnSourceCandidateAvailable(
        /*target=*/std::nullopt,
        OtpCandidate(*result, origin, LocalFrameToken()));
  }
}

bool GmailOtpSource::ProcessMatchedToken(
    const url::Origin& origin,
    const one_time_tokens::OneTimeToken& token) {
  CHECK_EQ(token.type(), one_time_tokens::OneTimeTokenType::kGmail);
  if (origin.opaque() || token.value().empty() ||
      IsTokenExpired(token, base::TimeTicks::Now())) {
    return false;
  }

  if (const one_time_tokens::OneTimeToken* existing_token =
          base::FindOrNull(verified_store_per_origin_, origin)) {
    if (!OtpTokenIdentity::FromOneTimeToken(token).IsStrictlyNewerThan(
            OtpTokenIdentity::FromOneTimeToken(*existing_token))) {
      return false;
    }
  }
  verified_store_per_origin_.insert_or_assign(origin, token);
  return true;
}

bool GmailOtpSource::HasActiveSubscriptionForOrigin(
    const url::Origin& origin) const {
  if (origin.opaque() || !UserOptedIntoGmailOtpFilling()) {
    return false;
  }
  const one_time_tokens::ExpiringSubscription* subscription =
      base::FindOrNull(continuous_subscriptions_per_origin_, origin);
  return subscription && subscription->IsAlive();
}

bool GmailOtpSource::UserOptedIntoGmailOtpFilling() const {
  PrefService* prefs = client_->GetPrefs();
  return prefs && prefs::IsAutofillGmailOtpFillingEnabled(prefs);
}

void GmailOtpSource::PruneExpiredConsumedTokens(base::TimeTicks now) {
  consumed_tokens_.erase(
      consumed_tokens_.begin(),
      consumed_tokens_.lower_bound(now - kGmailOtpCacheDuration));
}

}  // namespace autofill
