// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_retriever.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "url/origin.h"

namespace one_time_tokens {

namespace {

void RecordSenderDomainMatchRejectionReason(
    GmailOtpSenderDomainMatchType reason,
    bool is_cached) {
  if (is_cached) {
    base::UmaHistogramEnumeration(
        "OneTimeTokens.GmailOtpRetriever."
        "SenderDomainMatchRejectionReason.Cached",
        reason);
  } else {
    base::UmaHistogramEnumeration(
        "OneTimeTokens.GmailOtpRetriever."
        "SenderDomainMatchRejectionReason.Received",
        reason);
  }
}

void RecordSenderDomainMatchAcceptedMatchType(
    GmailOtpSenderDomainMatchType match_type,
    bool is_cached) {
  if (is_cached) {
    base::UmaHistogramEnumeration(
        "OneTimeTokens.GmailOtpRetriever."
        "SenderDomainMatchAcceptedMatchType.Cached",
        match_type);
  } else {
    base::UmaHistogramEnumeration(
        "OneTimeTokens.GmailOtpRetriever."
        "SenderDomainMatchAcceptedMatchType.Received",
        match_type);
  }
}

}  // namespace

// static
std::unique_ptr<GmailOtpRetriever> GmailOtpRetriever::CreateAndStart(
    OneTimeTokenService& service,
    std::unique_ptr<affiliations::DomainRelationChecker>
        domain_relation_checker,
    const url::Origin& otp_frame_origin,
    bool is_login_flow,
    ResultCallback callback) {
  auto retriever = base::WrapUnique(new GmailOtpRetriever(
      service, std::move(domain_relation_checker), otp_frame_origin,
      is_login_flow, std::move(callback)));
  retriever->Start();
  return retriever;
}

GmailOtpRetriever::GmailOtpRetriever(
    OneTimeTokenService& service,
    std::unique_ptr<affiliations::DomainRelationChecker>
        domain_relation_checker,
    const url::Origin& otp_frame_origin,
    bool is_login_flow,
    ResultCallback callback)
    : one_time_token_service_(service),
      otp_frame_origin_(otp_frame_origin),
      is_login_flow_(is_login_flow),
      sender_domain_matcher_(std::move(domain_relation_checker),
                             otp_frame_origin),
      retrieve_otp_callback_(std::move(callback)) {}

GmailOtpRetriever::~GmailOtpRetriever() = default;

void GmailOtpRetriever::Start() {
  if (otp_frame_origin_.opaque()) {
    // Post the task to the current object's weak pointer instead of posting the
    // callback directly. This ensures the callback is cancelled if this
    // retriever is destroyed (e.g., if a new request supersedes it).
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GmailOtpRetriever::OnOpaqueOriginDetected,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Note: OneTimeTokenService caches tokens for 1 minute. It does not clear
  // them upon use. If a user triggers a "Resend OTP" flow within that 1
  // minute, this will return the originally cached token rather than waiting
  // for the new one. This relies on the assumption that previously sent tokens
  // typically remain valid for the duration of the cache.
  std::vector<OneTimeToken> cached_tokens;
  // `GetRecentOneTimeTokens()` is synchronous, making it safe to pass a
  // reference to the local `cached_tokens` stack variable.
  one_time_token_service_->GetRecentOneTimeTokens(base::BindRepeating(
      [](std::vector<OneTimeToken>& tokens, OneTimeTokenSource source,
         base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
        if (source == OneTimeTokenSource::kGmail && result.has_value()) {
          tokens.push_back(std::move(*result));
        }
      },
      std::ref(cached_tokens)));

  // The cache checking is async, so also listen to the service in the meantime
  // in case the matching token is not in the cache. The tokens arriving from
  // the service are also checked for relevance.
  SubscribeForOneTimeToken();

  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever checking " << cached_tokens.size()
      << " cached Gmail token(s).";

  if (cached_tokens.empty()) {
    return;
  }

  std::ranges::sort(
      cached_tokens, [](const OneTimeToken& lhs, const OneTimeToken& rhs) {
        return lhs.email_received_timestamp() > rhs.email_received_timestamp();
      });

  CheckCachedTokenMatch(std::move(cached_tokens), /*index=*/0);
}

void GmailOtpRetriever::SubscribeForOneTimeToken() {
  // Subscribe to OneTimeTokenService with configurable period.
  base::TimeDelta subscription_period =
      features::kGmailOtpSubscriptionPeriodParam.Get();
  subscription_ = one_time_token_service_->Subscribe(
      OneTimeTokenSource::kGmail, base::Time::Now() + subscription_period,
      base::BindRepeating(&GmailOtpRetriever::OnOneTimeTokenReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GmailOtpRetriever::OnOneTimeTokenTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GmailOtpRetriever::StartSenderDomainCheck(
    std::string_view sender_address,
    GmailOtpSenderDomainMatcher::ResultCallback callback) {
  CHECK(!otp_frame_origin_.opaque());

  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever checking sender domain match: sender_address="
      << sender_address << ", otp_frame_origin=" << otp_frame_origin_;

  pending_sender_domain_checks_++;
  sender_domain_matcher_.Check(sender_address, std::move(callback));
}

void GmailOtpRetriever::CheckCachedTokenMatch(
    std::vector<OneTimeToken> cached_tokens,
    size_t index) {
  // If a racing check found a match already it would have invalidated
  // all the weak pointers for the other checks including this one, so this
  // wouldn't be called.
  CHECK(retrieve_otp_callback_);
  if (index >= cached_tokens.size()) {
    return;
  }

  std::string sender_address =
      cached_tokens.at(index).sender_address().value_or("");
  StartSenderDomainCheck(
      sender_address,
      base::BindOnce(&GmailOtpRetriever::OnCachedTokenMatchChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cached_tokens),
                     index));
}

bool GmailOtpRetriever::IsMatchTypeAllowed(
    GmailOtpSenderDomainMatchType match_type) const {
  switch (match_type) {
    case GmailOtpSenderDomainMatchType::kExact:
      LOG_OTT(one_time_token_service_->log_sink())
          << "GmailOtpRetriever exact match";
      return true;
    case GmailOtpSenderDomainMatchType::kFrameIsWwwPsl:
      // This is a particular case of PSL matching that is considered
      // a strong match.
      LOG_OTT(one_time_token_service_->log_sink())
          << "GmailOtpRetriever frame is www PSL match";
      return true;
    case GmailOtpSenderDomainMatchType::kAffiliated:
      LOG_OTT(one_time_token_service_->log_sink())
          << "GmailOtpRetriever affiliated match";
      return true;
    case GmailOtpSenderDomainMatchType::kPsl:
    case GmailOtpSenderDomainMatchType::kGroupedAndPsl:
      // PSL matches are allowed for login flows because the user already
      // expressed the intention to fill the target frame, by approving the
      // login flow.
      if (is_login_flow_) {
        LOG_OTT(one_time_token_service_->log_sink())
            << "GmailOtpRetriever PSL match during login flow";
      }
      return is_login_flow_;
    case GmailOtpSenderDomainMatchType::kUnknown:
    case GmailOtpSenderDomainMatchType::kNoMatch:
    case GmailOtpSenderDomainMatchType::kGrouped:
      return false;
  }
}

void GmailOtpRetriever::OnCachedTokenMatchChecked(
    std::vector<OneTimeToken> cached_tokens,
    size_t index,
    GmailOtpSenderDomainMatchType match_type) {
  // If the retriever had already completed, all weak pointers would have been
  // invalidated, so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  // Decrement early to ensure the counter stays reliably accurate regardless of
  // whether the match succeeds or fails.
  pending_sender_domain_checks_--;

  bool allowed = IsMatchTypeAllowed(match_type);
  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever cached token match checked: allowed=" << allowed
      << ", match_type=" << match_type << ", is_login_flow=" << is_login_flow_;
  if (allowed) {
    RecordSenderDomainMatchAcceptedMatchType(match_type, /*is_cached=*/true);
    const OneTimeToken& matched_token = cached_tokens.at(index);
    if (!best_candidate_.has_value() ||
        matched_token.email_received_timestamp().value_or(base::Time()) >=
            best_candidate_->email_received_timestamp) {
      best_candidate_ = Candidate{
          .otp = matched_token.value(),
          .source = Source::kCache,
          .email_received_timestamp =
              matched_token.email_received_timestamp().value_or(base::Time()),
      };
    }
    MaybeCompleteOrWaitForPendingRequests();
    return;
  }

  RecordSenderDomainMatchRejectionReason(match_type, /*is_cached=*/true);

  // Since `cached_tokens` is sorted descending by email received timestamp in
  // `Start()`, only check the next cached token if we don't already have a
  // candidate with an email received timestamp >= the remaining cached tokens.
  if (index + 1 < cached_tokens.size()) {
    std::optional<base::Time> next_email_received_timestamp =
        cached_tokens.at(index + 1).email_received_timestamp();
    if (!best_candidate_.has_value() ||
        next_email_received_timestamp.value_or(base::Time()) >
            best_candidate_->email_received_timestamp) {
      CheckCachedTokenMatch(std::move(cached_tokens), index + 1);
    }
  }

  MaybeCompleteOrWaitForPendingRequests();
}

void GmailOtpRetriever::OnOneTimeTokenReceived(
    OneTimeTokenSource source,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
  CHECK_EQ(source, OneTimeTokenSource::kGmail);
  // If the retriever had already completed, all weak pointers would have been
  // invalidated, so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  if (!result.has_value()) {
    LOG_OTT(one_time_token_service_->log_sink())
        << "GmailOtpRetriever received error from service: error="
        << static_cast<int>(result.error());
    error_ = result.error();
    MaybeCompleteOrWaitForPendingRequests();
    return;
  }

  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever received token from service: sender_address="
      << result->sender_address().value_or("");

  std::string sender_address = result->sender_address().value_or("");
  StartSenderDomainCheck(
      sender_address,
      base::BindOnce(&GmailOtpRetriever::OnReceivedTokenMatchChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(*result)));
}

void GmailOtpRetriever::OnReceivedTokenMatchChecked(
    OneTimeToken token,
    GmailOtpSenderDomainMatchType match_type) {
  // If the retriever had already completed, all weak pointers would have been
  // invalidated, so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  // Decrement early to ensure the counter stays reliably accurate regardless of
  // whether the match succeeds or fails.
  pending_sender_domain_checks_--;

  bool allowed = IsMatchTypeAllowed(match_type);
  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever received token match checked: allowed=" << allowed
      << ", match_type=" << match_type << ", is_login_flow=" << is_login_flow_;
  if (allowed) {
    RecordSenderDomainMatchAcceptedMatchType(match_type, /*is_cached=*/false);
    if (!best_candidate_.has_value() ||
        token.email_received_timestamp().value_or(base::Time()) >=
            best_candidate_->email_received_timestamp) {
      best_candidate_ = Candidate{
          .otp = token.value(),
          .source = Source::kReceived,
          .email_received_timestamp =
              token.email_received_timestamp().value_or(base::Time()),
      };
    }
  } else {
    RecordSenderDomainMatchRejectionReason(match_type, /*is_cached=*/false);
  }

  MaybeCompleteOrWaitForPendingRequests();
}

void GmailOtpRetriever::OnOneTimeTokenTimeout() {
  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever subscription timed out waiting for matching OTP.";

  // The retriever will no longer be called after timeout anyway, but
  // clean up the state nonetheless to make it clearer.
  subscription_ = {};
  if (!error_.has_value()) {
    error_ = OneTimeTokenRetrievalError::kSubscriptionExpired;
  }
  MaybeCompleteOrWaitForPendingRequests();
}

void GmailOtpRetriever::MaybeCompleteOrWaitForPendingRequests() {
  CHECK(retrieve_otp_callback_);

  // Always wait for in-flight domain checks (initiated before timeout or from
  // cache).
  if (pending_sender_domain_checks_ > 0) {
    return;
  }

  // Only wait for pending backend requests if the subscription is still alive,
  // which has a timeout of its own.
  if (subscription_.IsAlive() &&
      one_time_token_service_->HasPendingRequests(OneTimeTokenSource::kGmail)) {
    return;
  }

  if (best_candidate_.has_value()) {
    subscription_ = {};
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(retrieve_otp_callback_)
        .Run(Result{
            .otp = std::move(best_candidate_->otp),
            .source = best_candidate_->source,
        });
    return;
  }

  if (error_.has_value()) {
    subscription_ = {};
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(retrieve_otp_callback_).Run(base::unexpected(*error_));
  }
}

void GmailOtpRetriever::OnOpaqueOriginDetected() {
  CHECK(retrieve_otp_callback_);
  LOG_OTT(one_time_token_service_->log_sink())
      << "GmailOtpRetriever failed: Opaque frame origin.";
  std::move(retrieve_otp_callback_)
      .Run(base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown));
}

std::ostream& operator<<(std::ostream& os, GmailOtpRetriever::Source source) {
  switch (source) {
    case GmailOtpRetriever::Source::kCache:
      return os << "kCache";
    case GmailOtpRetriever::Source::kReceived:
      return os << "kReceived";
  }
  return os << static_cast<int>(source);
}

}  // namespace one_time_tokens
