// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/fake_gmail_otp_service.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace one_time_tokens {

FakeGmailOtpService::FakeGmailOtpService() = default;

FakeGmailOtpService::~FakeGmailOtpService() = default;

GmailOtpBackend* FakeGmailOtpService::GetBackend() {
  return &backend_;
}

void FakeGmailOtpService::GetFreshestCachedOtp(
    const url::Origin& origin,
    bool allow_psl_matching,
    GetFreshestCachedOtpCallback callback) {
  ++num_get_freshest_cached_otp_calls_;
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  std::optional<OneTimeToken> freshest_token;
  for (const OneTimeToken& token : cached_tokens_) {
    if (FrameMatchesSenderEmail(origin, allow_psl_matching,
                                token.sender_address())) {
      if (!freshest_token.has_value() ||
          token.email_received_timestamp().value_or(base::Time()) >
              freshest_token->email_received_timestamp().value_or(
                  base::Time())) {
        freshest_token = token;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(freshest_token)));
}

ExpiringSubscription FakeGmailOtpService::Subscribe(
    const url::Origin& origin,
    bool allow_psl_matching,
    base::Time expiration,
    IncomingTokenCallback callback,
    base::OnceClosure expiration_callback) {
  ++num_subscribe_calls_;
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            callback,
            base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown)));
  }
  return subscription_manager_.Subscribe(
      expiration,
      base::BindRepeating(&FakeGmailOtpService::NotifySubscriber,
                          weak_ptr_factory_.GetWeakPtr(), origin,
                          allow_psl_matching, std::move(callback)),
      std::move(expiration_callback));
}

bool FakeGmailOtpService::HasPendingRequests(
    const ExpiringSubscription& subscription) const {
  return has_pending_requests_;
}

OneTimeTokenLogSink* FakeGmailOtpService::GetLogSink() {
  return backend_.GetLogSink();
}

void FakeGmailOtpService::ProcessCallbacks(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  if (reply.has_value()) {
    cached_tokens_.push_back(*reply);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeGmailOtpService::DispatchCallbacks,
                                weak_ptr_factory_.GetWeakPtr(), reply));
  backend_.ProcessCallbacks(std::move(reply));
}

void FakeGmailOtpService::ProcessCallbacks(OneTimeToken token) {
  ProcessCallbacks(base::ok(std::move(token)));
}

void FakeGmailOtpService::SetHasPendingRequests(bool has_pending_requests) {
  has_pending_requests_ = has_pending_requests;
  backend_.SetHasPendingRequests(has_pending_requests);
}

void FakeGmailOtpService::SetCachedTokens(std::vector<OneTimeToken> tokens) {
  cached_tokens_ = std::move(tokens);
}

void FakeGmailOtpService::ClearCache() {
  cached_tokens_.clear();
  backend_.ClearCache();
}

size_t FakeGmailOtpService::num_callbacks() const {
  return subscription_manager_.GetNumberSubscribers() +
         backend_.num_callbacks();
}

void FakeGmailOtpService::NotifySubscriber(
    const url::Origin& origin,
    bool allow_psl_matching,
    IncomingTokenCallback callback,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  if (origin.opaque()) {
    return;
  }
  if (!reply.has_value()) {
    callback.Run(std::move(reply));
    return;
  }
  if (FrameMatchesSenderEmail(origin, allow_psl_matching,
                              reply->sender_address())) {
    callback.Run(std::move(reply));
  } else {
    callback.Run(base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpSenderDomainMismatch));
  }
}

// static
bool FakeGmailOtpService::FrameMatchesSenderEmail(
    const url::Origin& frame_origin,
    bool allow_psl_matching,
    std::optional<std::string_view> sender_email) {
  if (!sender_email || sender_email->empty()) {
    return true;
  }
  size_t at_pos = sender_email->find('@');
  std::string_view domain = (at_pos != std::string_view::npos)
                                ? sender_email->substr(at_pos + 1)
                                : *sender_email;
  if (frame_origin.DomainIs(domain)) {
    return true;
  }
  if (allow_psl_matching) {
    return net::registry_controlled_domains::SameDomainOrHost(
        frame_origin,
        url::Origin::Create(GURL(base::StrCat({"https://", domain}))),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  }
  return false;
}

void FakeGmailOtpService::DispatchCallbacks(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  subscription_manager_.Notify(reply);
}

}  // namespace one_time_tokens
