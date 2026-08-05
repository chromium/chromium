// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_retriever.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/match_type.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace one_time_tokens {

namespace {

std::string ExtractEmailDomain(std::string_view email) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      email, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() == 2) {
    return std::string(parts[1]);
  }
  return std::string();
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
      domain_relation_checker_(std::move(domain_relation_checker)),
      otp_frame_origin_(otp_frame_origin),
      is_login_flow_(is_login_flow),
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

  // Note: OneTimeTokenService caches tokens for 3 minutes. It does not clear
  // them upon use. If a user triggers a "Resend OTP" flow within those 3
  // minutes, this will return the originally cached token rather than waiting
  // for the new one. This relies on the assumption that previously sent tokens
  // typically remain valid for the duration of the cache.
  std::vector<OneTimeToken> cached_tokens;
  for (const auto& token : one_time_token_service_->GetCachedOneTimeTokens()) {
    if (token.type() == OneTimeTokenType::kGmail) {
      cached_tokens.push_back(token);
    }
  }

  // The cache checking is async, so also listen to the service in the meantime
  // in case the matching token is not in the cache. The tokens arriving from
  // the service are also checked for relevance.
  SubscribeForOneTimeToken();

  if (cached_tokens.empty()) {
    return;
  }

  std::ranges::sort(cached_tokens, [](const auto& lhs, const auto& rhs) {
    return lhs.on_device_arrival_time() > rhs.on_device_arrival_time();
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

void GmailOtpRetriever::CheckSenderDomainMatchesFrameToFill(
    std::string_view sender_address,
    base::OnceCallback<void(std::optional<affiliations::MatchType>)> callback) {
  pending_sender_domain_checks_++;
  std::string sender_domain = ExtractEmailDomain(sender_address);

  CHECK(!otp_frame_origin_.opaque());
  domain_relation_checker_->Check(
      otp_frame_origin_.GetTupleOrPrecursorTupleIfOpaque(),
      url::SchemeHostPort(url::kHttpsScheme, std::move(sender_domain),
                          url::DefaultPortForScheme(url::kHttpsScheme)),
      std::move(callback));
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
  CheckSenderDomainMatchesFrameToFill(
      sender_address,
      base::BindOnce(&GmailOtpRetriever::OnCachedTokenMatchChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cached_tokens),
                     index));
}

bool GmailOtpRetriever::IsMatchTypeAllowed(
    std::optional<affiliations::MatchType> match_type) const {
  if (!match_type.has_value()) {
    return false;
  }
  bool is_exact_or_affiliated =
      (*match_type == affiliations::MatchType::kExact) ||
      (static_cast<int>(*match_type) &
       static_cast<int>(affiliations::MatchType::kAffiliated));
  if (is_exact_or_affiliated) {
    return true;
  }
  bool is_psl = static_cast<int>(*match_type) &
                static_cast<int>(affiliations::MatchType::kPSL);
  // PSL matches are allowed for login flows because the user already expressed
  // the intention to fill the target frame, by approving the login flow.
  return is_psl && is_login_flow_;
}

void GmailOtpRetriever::OnCachedTokenMatchChecked(
    std::vector<OneTimeToken> cached_tokens,
    size_t index,
    std::optional<affiliations::MatchType> match_type) {
  // If a racing check found a match already it would have invalidated
  // all weak pointers for other checks so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  // Decrement early to ensure the counter stays reliably accurate regardless of
  // whether the match succeeds or fails. If a match is found, weak pointers are
  // synchronously invalidated below, preventing any artificial timeout races.
  pending_sender_domain_checks_--;

  if (IsMatchTypeAllowed(match_type)) {
    subscription_ = {};
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(retrieve_otp_callback_)
        .Run(Result{
            .otp = cached_tokens.at(index).value(),
            .source = Source::kCache,
        });
    return;
  }

  CheckCachedTokenMatch(std::move(cached_tokens), index + 1);
  MaybeFail();
}

void GmailOtpRetriever::OnOneTimeTokenReceived(
    OneTimeTokenSource source,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
  CHECK_EQ(source, OneTimeTokenSource::kGmail);
  // If a racing check found a match already it would have invalidated
  // the weak pointer for the on-token-received callback and this wouldn't be
  // called.
  CHECK(retrieve_otp_callback_);

  if (!result.has_value()) {
    error_ = result.error();
    subscription_ = {};
    MaybeFail();
    return;
  }

  std::string sender_address = result->sender_address().value_or("");
  CheckSenderDomainMatchesFrameToFill(
      sender_address,
      base::BindOnce(&GmailOtpRetriever::OnReceivedTokenMatchChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(*result)));
}

void GmailOtpRetriever::OnReceivedTokenMatchChecked(
    OneTimeToken token,
    std::optional<affiliations::MatchType> match_type) {
  // If a previous check found a match already it would have invalidated
  // the weak pointer for this callback, so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  // Decrement early to ensure the counter stays reliably accurate regardless of
  // whether the match succeeds or fails. If a match is found, weak pointers are
  // synchronously invalidated below, preventing any artificial timeout races.
  pending_sender_domain_checks_--;

  if (IsMatchTypeAllowed(match_type)) {
    subscription_ = {};
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(retrieve_otp_callback_)
        .Run(Result{
            .otp = token.value(),
            .source = Source::kReceived,
        });
    return;
  }

  MaybeFail();
}

void GmailOtpRetriever::OnOneTimeTokenTimeout() {
  // The retriever will no longer be called after timeout anyway, but
  // clean up the state nonetheless to make it clearer.
  subscription_ = {};
  error_ = OneTimeTokenRetrievalError::kSubscriptionExpired;
  MaybeFail();
}

void GmailOtpRetriever::MaybeFail() {
  if (pending_sender_domain_checks_ == 0 && error_.has_value()) {
    CHECK(retrieve_otp_callback_);
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(retrieve_otp_callback_).Run(base::unexpected(*error_));
  }
}

void GmailOtpRetriever::OnOpaqueOriginDetected() {
  CHECK(retrieve_otp_callback_);
  std::move(retrieve_otp_callback_)
      .Run(base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown));
}

}  // namespace one_time_tokens
