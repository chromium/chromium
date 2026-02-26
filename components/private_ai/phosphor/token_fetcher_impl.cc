// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/token_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/config_http.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/phosphor/oauth_token_provider.h"
#include "components/private_ai/phosphor/token_fetcher.h"
#include "components/private_ai/phosphor/token_fetcher_helper.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai::phosphor {

TokenFetcherImpl::SequenceBoundFetch::SequenceBoundFetch(
    std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth)
    : blind_sign_auth_(std::move(blind_sign_auth)) {}

TokenFetcherImpl::SequenceBoundFetch::~SequenceBoundFetch() = default;

void TokenFetcherImpl::SequenceBoundFetch::GetTokensFromBlindSignAuth(
    quiche::BlindSignAuthServiceType service_type,
    std::optional<std::string> access_token,
    int batch_size,
    quiche::ProxyLayer proxy_layer,
    TokenFetcherHelper::FetchBlindSignedTokenCallback callback) {
  CHECK(blind_sign_auth_);
  fetcher_helper_.GetTokensFromBlindSignAuth(
      blind_sign_auth_.get(), service_type, std::move(access_token), batch_size,
      proxy_layer, std::move(callback));
}

TokenFetcherImpl::TokenFetcherImpl(
    OAuthTokenProvider* oauth_token_provider,
    std::unique_ptr<quiche::BlindSignAuthInterface> bsa)
    : oauth_token_provider_(oauth_token_provider),
      thread_pool_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  helper_.emplace(thread_pool_task_runner_, std::move(bsa));
}

TokenFetcherImpl::~TokenFetcherImpl() = default;

void TokenFetcherImpl::GetAuthnTokens(int batch_size,
                                      quiche::ProxyLayer proxy_layer,
                                      GetAuthnTokensCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (batch_size <= 0) {
    LOG(ERROR) << "GetAuthnTokens called with non-positive batch_size.";
    GetAuthnTokensComplete(std::nullopt, std::move(callback),
                           GetAuthnTokensResult::kFailedBSA400);
    return;
  }
  // If we are in a state where the OAuth token has persistent errors then don't
  // try to request tokens.
  if (last_get_authn_tokens_backoff_ &&
      *last_get_authn_tokens_backoff_ == base::TimeDelta::Max()) {
    GetAuthnTokensComplete(std::nullopt, std::move(callback),
                           GetAuthnTokensResult::kFailedOAuthTokenPersistent);
    return;
  }

  auto request_token_callback = base::BindOnce(
      &TokenFetcherImpl::OnRequestOAuthTokenCompletedForGetAuthnTokens,
      weak_ptr_factory_.GetWeakPtr(), start_time, batch_size, proxy_layer,
      std::move(callback));

  oauth_token_provider_->RequestOAuthToken(std::move(request_token_callback));
}

void TokenFetcherImpl::OnRequestOAuthTokenCompletedForGetAuthnTokens(
    base::TimeTicks start_time,
    int batch_size,
    quiche::ProxyLayer proxy_layer,
    GetAuthnTokensCallback callback,
    GetAuthnTokensResult result,
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramMediumTimes(
      "PrivateAi.Phosphor.TokenFetcher.OAuthTokenFetchLatency",
      base::TimeTicks::Now() - start_time);

  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (!access_token) {
    CHECK(result != GetAuthnTokensResult::kSuccess);
    GetAuthnTokensComplete(std::nullopt, std::move(callback), result);
    return;
  }

  // Use `TerminalLayer` as the `ProxyLayer` parameter because we want tokens
  // for the PI server. This should be parameterized later when we introduce IP
  // protection and a proxy to fetch the needed tokens for the proxy layer.
  helper_
      .AsyncCall(
          &TokenFetcherImpl::SequenceBoundFetch::GetTokensFromBlindSignAuth)
      .WithArgs(quiche::BlindSignAuthServiceType::kChromePrivateAratea,
                std::move(access_token), batch_size, proxy_layer,
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &TokenFetcherImpl::OnFetchBlindSignedTokenCompleted,
                    weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void TokenFetcherImpl::OnFetchBlindSignedTokenCompleted(
    GetAuthnTokensCallback callback,
    base::expected<std::vector<quiche::BlindSignToken>, absl::Status> tokens) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using enum GetAuthnTokensResult;

  if (!tokens.has_value()) {
    // Apply the canonical mapping from abseil status to HTTP status.
    GetAuthnTokensResult result;
    switch (tokens.error().code()) {
      case absl::StatusCode::kInvalidArgument:
        result = kFailedBSA400;
        break;
      case absl::StatusCode::kUnauthenticated:
        result = kFailedBSA401;
        break;
      case absl::StatusCode::kPermissionDenied:
        result = kFailedBSA403;
        break;
      default:
        result = kFailedBSAOther;
        break;
    }
    GetAuthnTokensComplete(std::nullopt, std::move(callback), result);
    return;
  }

  if (tokens.value().empty()) {
    GetAuthnTokensComplete(std::nullopt, std::move(callback), kFailedBSAOther);
    return;
  }

  std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens =
      TokenFetcherHelper::QuicheTokensToPhosphorAuthTokens(tokens.value());
  if (!bsa_tokens) {
    GetAuthnTokensComplete(std::nullopt, std::move(callback), kFailedBSAOther);
    return;
  }

  GetAuthnTokensComplete(std::move(bsa_tokens), std::move(callback), kSuccess);
}

void TokenFetcherImpl::GetAuthnTokensComplete(
    std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens,
    GetAuthnTokensCallback callback,
    GetAuthnTokensResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramEnumeration(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result", result);

  std::optional<base::TimeDelta> backoff = CalculateBackoff(result);
  if (bsa_tokens.has_value()) {
    DCHECK(!backoff.has_value());
    std::move(callback).Run(base::ok(*std::move(bsa_tokens)));
    return;
  }

  DCHECK(backoff.has_value());
  const base::Time try_again_after = (*backoff == base::TimeDelta::Max())
                                         ? base::Time::Max()
                                         : base::Time::Now() + *backoff;
  std::move(callback).Run(base::unexpected(try_again_after));
}

void TokenFetcherImpl::AccountStatusChanged(bool account_available) {
  if (account_available) {
    // End the backoff period if it was caused by account-related issues.
    if (last_get_authn_tokens_backoff_ == base::TimeDelta::Max()) {
      last_get_authn_tokens_backoff_.reset();
    }
  } else {
    last_get_authn_tokens_backoff_ = base::TimeDelta::Max();
  }
}

std::optional<base::TimeDelta> TokenFetcherImpl::CalculateBackoff(
    GetAuthnTokensResult result) {
  using enum GetAuthnTokensResult;
  std::optional<base::TimeDelta> backoff;
  bool exponential = false;
  switch (result) {
    case kSuccess:
      break;
    case kFailedNoAccount:
    case kFailedOAuthTokenPersistent:
      backoff = base::TimeDelta::Max();
      break;
    case kFailedNotEligible:
    case kFailedBSA403:
      // Eligibility, whether determined locally or on the server, is unlikely
      // to change quickly.
      backoff = kPrivateAiTryGetAuthTokensNotEligibleBackoff.Get();
      break;
    case kFailedOAuthTokenTransient:
    case kFailedBSAOther:
      // Transient failure to fetch an OAuth token, or some other error from
      // BSA that is probably transient.
      backoff = kPrivateAiTryGetAuthTokensTransientBackoff.Get();
      exponential = true;
      break;
    case kFailedBSA400:
    case kFailedBSA401:
      // Both 400 and 401 suggest a bug, so do not retry aggressively.
      backoff = kPrivateAiTryGetAuthTokensBugBackoff.Get();
      exponential = true;
      break;
    case kFailedOAuthTokenDeprecated:
      NOTREACHED();
  }

  if (exponential) {
    if (last_get_authn_tokens_backoff_ &&
        last_get_authn_tokens_result_ == result) {
      backoff = *last_get_authn_tokens_backoff_ * 2;
    }
  }

  if (last_get_authn_tokens_backoff_ &&
      *last_get_authn_tokens_backoff_ == base::TimeDelta::Max()) {
    return *last_get_authn_tokens_backoff_;
  }

  last_get_authn_tokens_result_ = result;
  last_get_authn_tokens_backoff_ = backoff;

  if (backoff && backoff != base::TimeDelta::Max()) {
    return base::RandomizeByPercentage(*backoff,
                                       kPrivateAiBackoffJitter.Get() * 100);
  }
  return backoff;
}

}  // namespace private_ai::phosphor
