// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace one_time_tokens {

namespace {

bool IsMatchTypeAllowed(GmailOtpSenderDomainMatchType match_type,
                        bool allow_psl_matching,
                        OneTimeTokenLogSink* log_sink) {
  switch (match_type) {
    case GmailOtpSenderDomainMatchType::kExact:
      LOG_OTT(log_sink) << "GmailOtpService exact match";
      return true;
    case GmailOtpSenderDomainMatchType::kFrameIsWwwPsl:
      // This is a particular case of PSL matching that is considered
      // a strong match.
      LOG_OTT(log_sink) << "GmailOtpService frame is www PSL match";
      return true;
    case GmailOtpSenderDomainMatchType::kAffiliated:
      LOG_OTT(log_sink) << "GmailOtpService affiliated match";
      return true;
    case GmailOtpSenderDomainMatchType::kPsl:
    case GmailOtpSenderDomainMatchType::kGroupedAndPsl:
      if (allow_psl_matching) {
        LOG_OTT(log_sink) << "GmailOtpService PSL match";
      }
      return allow_psl_matching;
    case GmailOtpSenderDomainMatchType::kUnknown:
    case GmailOtpSenderDomainMatchType::kNoMatch:
    case GmailOtpSenderDomainMatchType::kGrouped:
      return false;
  }
}

void RecordSenderDomainMatchRejectionReason(
    GmailOtpSenderDomainMatchType reason,
    bool is_cached,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration(
      is_cached ? "OneTimeTokens.GmailNonAgentic."
                  "SenderDomainMatchRejectionReason.Cached"
                : "OneTimeTokens.GmailNonAgentic."
                  "SenderDomainMatchRejectionReason.Received",
      reason);
  if (ukm_source_id != ukm::kInvalidSourceId) {
    ukm::builders::OneTimeTokens builder(ukm_source_id);
    if (is_cached) {
      builder.SetGmail_SenderDomainMatchRejectionReason_Cached(
          std::to_underlying(reason));
    } else {
      builder.SetGmail_SenderDomainMatchRejectionReason_Received(
          std::to_underlying(reason));
    }
    builder.Record(ukm::UkmRecorder::Get());
  }
}

void RecordSenderDomainMatchAcceptedMatchType(
    GmailOtpSenderDomainMatchType match_type,
    bool is_cached,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration(
      is_cached ? "OneTimeTokens.GmailNonAgentic."
                  "SenderDomainMatchAcceptedMatchType.Cached"
                : "OneTimeTokens.GmailNonAgentic."
                  "SenderDomainMatchAcceptedMatchType.Received",
      match_type);
  if (ukm_source_id != ukm::kInvalidSourceId) {
    ukm::builders::OneTimeTokens builder(ukm_source_id);
    if (is_cached) {
      builder.SetGmail_SenderDomainMatchAcceptedMatchType_Cached(
          std::to_underlying(match_type));
    } else {
      builder.SetGmail_SenderDomainMatchAcceptedMatchType_Received(
          std::to_underlying(match_type));
    }
    builder.Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace

struct GmailOtpServiceImpl::ScopedDomainMatcherCleanup {
  ~ScopedDomainMatcherCleanup() {
    if (service) {
      service->domain_matchers_.erase(handle);
    }
  }

  base::WeakPtr<GmailOtpServiceImpl> service;
  ExpiringSubscriptionHandle handle;
};

GmailOtpServiceImpl::GmailOtpServiceImpl(
    GmailOtpBackend* backend,
    affiliations::AffiliationService* affiliation_service,
    std::optional<OneTimeTokenLogSink::LogHandler> log_handler)
    : backend_(backend),
      domain_relation_checker_(CHECK_DEREF(affiliation_service)) {
  CHECK(backend);
  if (log_handler && backend_->GetLogSink()) {
    log_subscription_ =
        backend_->GetLogSink()->AddLogHandler(std::move(*log_handler));
  }
}

GmailOtpServiceImpl::~GmailOtpServiceImpl() = default;

GmailOtpBackend* GmailOtpServiceImpl::GetBackend() {
  return backend_;
}

void GmailOtpServiceImpl::GetFreshestCachedOtp(
    const url::Origin& origin,
    ukm::SourceId ukm_source_id,
    bool allow_psl_matching,
    GetFreshestCachedOtpCallback callback) {
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Note: GmailOtpBackend caches tokens for 1 minute. It does not clear
  // them upon use. If a user triggers a "Resend OTP" flow within that 1
  // minute, this will return the originally cached token rather than waiting
  // for the new one. This relies on the assumption that previously sent tokens
  // typically remain valid for the duration of the cache.
  std::vector<OneTimeToken> cached_tokens =
      backend_->PurgeExpiredAndGetCachedOneTimeTokens();
  LOG_OTT(backend_->GetLogSink())
      << "GmailOtpService checking " << cached_tokens.size()
      << " cached Gmail token(s).";
  if (cached_tokens.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  std::ranges::sort(
      cached_tokens, [](const OneTimeToken& lhs, const OneTimeToken& rhs) {
        return lhs.email_received_timestamp() > rhs.email_received_timestamp();
      });

  ExpiringSubscriptionHandle check_handle(base::Uuid::GenerateRandomV4());
  domain_matchers_.emplace(check_handle,
                           std::make_unique<GmailOtpSenderDomainMatcher>(
                               &domain_relation_checker_, origin));
  StartCachedOtpCheck(std::move(check_handle), std::move(cached_tokens),
                      /*current_index=*/0, ukm_source_id, allow_psl_matching,
                      std::move(callback));
}

void GmailOtpServiceImpl::StartCachedOtpCheck(
    ExpiringSubscriptionHandle check_handle,
    std::vector<OneTimeToken> tokens,
    size_t current_index,
    ukm::SourceId ukm_source_id,
    bool allow_psl_matching,
    GetFreshestCachedOtpCallback callback) {
  if (current_index >= tokens.size()) {
    domain_matchers_.erase(check_handle);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  auto it = domain_matchers_.find(check_handle);
  CHECK(it != domain_matchers_.end());
  GmailOtpSenderDomainMatcher& domain_matcher = *it->second;

  std::string sender_address =
      tokens[current_index].sender_address().value_or("");
  LOG_OTT(backend_->GetLogSink())
      << "GmailOtpService checking sender domain match: sender_address="
      << sender_address
      << ", otp_frame_origin=" << domain_matcher.otp_frame_origin();
  domain_matcher.Check(
      sender_address,
      base::BindOnce(&GmailOtpServiceImpl::OnCachedTokenMatchChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(check_handle),
                     std::move(tokens), current_index, ukm_source_id,
                     allow_psl_matching, std::move(callback)));
}

void GmailOtpServiceImpl::OnCachedTokenMatchChecked(
    ExpiringSubscriptionHandle check_handle,
    std::vector<OneTimeToken> tokens,
    size_t current_index,
    ukm::SourceId ukm_source_id,
    bool allow_psl_matching,
    GetFreshestCachedOtpCallback callback,
    GmailOtpSenderDomainMatchType match_type) {
  bool allowed = IsMatchTypeAllowed(match_type, allow_psl_matching,
                                    backend_->GetLogSink());
  LOG_OTT(backend_->GetLogSink())
      << "GmailOtpService cached token match checked: allowed=" << allowed
      << ", match_type=" << match_type
      << ", allow_psl_matching=" << allow_psl_matching;
  if (allowed) {
    domain_matchers_.erase(check_handle);
    RecordSenderDomainMatchAcceptedMatchType(match_type, /*is_cached=*/true,
                                             ukm_source_id);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::make_optional(std::move(tokens[current_index]))));
    return;
  }

  RecordSenderDomainMatchRejectionReason(match_type, /*is_cached=*/true,
                                         ukm_source_id);
  StartCachedOtpCheck(std::move(check_handle), std::move(tokens),
                      current_index + 1, ukm_source_id, allow_psl_matching,
                      std::move(callback));
}

ExpiringSubscription GmailOtpServiceImpl::Subscribe(
    const url::Origin& origin,
    ukm::SourceId ukm_source_id,
    bool allow_psl_matching,
    base::Time expiration,
    IncomingTokenCallback callback,
    base::OnceClosure expiration_callback) {
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            callback,
            base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown)));
    return subscription_manager_.Subscribe(expiration, base::DoNothing(),
                                           std::move(expiration_callback));
  }

  auto cleanup = std::make_unique<ScopedDomainMatcherCleanup>();
  ScopedDomainMatcherCleanup* cleanup_ptr = cleanup.get();

  ExpiringSubscription subscription = subscription_manager_.Subscribe(
      expiration,
      base::BindRepeating(&GmailOtpServiceImpl::NotifySubscriber,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Owned(std::move(cleanup)), ukm_source_id,
                          allow_psl_matching, std::move(callback)),
      std::move(expiration_callback));

  cleanup_ptr->service = weak_ptr_factory_.GetWeakPtr();
  cleanup_ptr->handle = subscription.handle();
  domain_matchers_.emplace(subscription.handle(),
                           std::make_unique<GmailOtpSenderDomainMatcher>(
                               &domain_relation_checker_, origin));

  EnsureBackendSubscription(expiration);

  return subscription;
}

bool GmailOtpServiceImpl::HasPendingRequests(
    const ExpiringSubscription& subscription) const {
  if (backend_->HasPendingRequests()) {
    return true;
  }
  auto it = domain_matchers_.find(subscription.handle());
  return it != domain_matchers_.end() && it->second->HasPendingChecks();
}

OneTimeTokenLogSink* GmailOtpServiceImpl::GetLogSink() {
  return backend_->GetLogSink();
}

void GmailOtpServiceImpl::EnsureBackendSubscription(base::Time expiration) {
  if (!backend_subscription_.IsAlive()) {
    backend_subscription_ = backend_->Subscribe(
        expiration,
        base::BindRepeating(&GmailOtpServiceImpl::OnBackendTokenReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&GmailOtpServiceImpl::OnBackendSubscriptionExpired,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (expiration > backend_subscription_.GetExpirationTime()) {
    backend_subscription_.SetExpirationTime(expiration);
  }
}

void GmailOtpServiceImpl::NotifySubscriber(
    const ScopedDomainMatcherCleanup* cleanup,
    ukm::SourceId ukm_source_id,
    bool allow_psl_matching,
    IncomingTokenCallback callback,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
  if (!result.has_value()) {
    callback.Run(base::unexpected(result.error()));
    return;
  }

  auto it = domain_matchers_.find(cleanup->handle);
  CHECK(it != domain_matchers_.end());
  GmailOtpSenderDomainMatcher& domain_matcher = *it->second;

  OneTimeToken token = std::move(*result);
  std::string sender_address = token.sender_address().value_or("");
  LOG_OTT(backend_->GetLogSink())
      << "GmailOtpService checking sender domain match: sender_address="
      << sender_address
      << ", otp_frame_origin=" << domain_matcher.otp_frame_origin();
  domain_matcher.Check(
      sender_address,
      base::BindOnce(&GmailOtpServiceImpl::OnSubscriberDomainMatchChecked,
                     weak_ptr_factory_.GetWeakPtr(), ukm_source_id,
                     allow_psl_matching, std::move(callback),
                     std::move(token)));
}

void GmailOtpServiceImpl::OnBackendTokenReceived(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
  subscription_manager_.Notify(result);
}

void GmailOtpServiceImpl::OnBackendSubscriptionExpired() {
  backend_subscription_ = {};
  if (subscription_manager_.GetNumberSubscribers() > 0) {
    base::Time latest_expiration =
        subscription_manager_.GetLatestExpirationTime();
    if (latest_expiration > base::Time::Now()) {
      EnsureBackendSubscription(latest_expiration);
    }
  }
}

void GmailOtpServiceImpl::OnSubscriberDomainMatchChecked(
    ukm::SourceId ukm_source_id,
    bool allow_psl_matching,
    IncomingTokenCallback callback,
    OneTimeToken token,
    GmailOtpSenderDomainMatchType match_type) {
  bool allowed = IsMatchTypeAllowed(match_type, allow_psl_matching,
                                    backend_->GetLogSink());
  LOG_OTT(backend_->GetLogSink())
      << "GmailOtpService received token match checked: allowed=" << allowed
      << ", match_type=" << match_type
      << ", allow_psl_matching=" << allow_psl_matching;
  if (allowed) {
    RecordSenderDomainMatchAcceptedMatchType(match_type, /*is_cached=*/false,
                                             ukm_source_id);
    callback.Run(base::ok(std::move(token)));
  } else {
    RecordSenderDomainMatchRejectionReason(match_type, /*is_cached=*/false,
                                           ukm_source_id);
    callback.Run(base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpSenderDomainMismatch));
  }
}

}  // namespace one_time_tokens
