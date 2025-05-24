// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android/ip_protection_token_ipc_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/ip_protection/android/blind_sign_message_android_impl.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_fetcher_helper.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/features.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace ip_protection {

IpProtectionTokenIpcFetcher::IpProtectionTokenIpcFetcher(
    Delegate* delegate,
    std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_for_testing)
    : delegate_(delegate),
      thread_pool_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      helper_(thread_pool_task_runner_) {
  blind_sign_message_android_impl_ =
      std::make_unique<ip_protection::BlindSignMessageAndroidImpl>();
  // TODO(b/360340499) : Remove `blind_sign_auth_for_testing` and implement mock
  // fetcher for unit tests.
  if (blind_sign_auth_for_testing) {
    blind_sign_auth_ = std::move(blind_sign_auth_for_testing);
    return;
  }
  privacy::ppn::BlindSignAuthOptions bsa_options{};
  bsa_options.set_enable_privacy_pass(true);

  blind_sign_auth_ = std::make_unique<quiche::BlindSignAuth>(
      blind_sign_message_android_impl_.get(), std::move(bsa_options));
}

IpProtectionTokenIpcFetcher::~IpProtectionTokenIpcFetcher() = default;

void IpProtectionTokenIpcFetcher::TryGetAuthTokens(
    uint32_t batch_size,
    ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If IP Protection is disabled then don't attempt to fetch tokens.
  if (!delegate_->IsTokenFetchEnabled()) {
    TryGetAuthTokensComplete(
        /*bsa_tokens=*/std::nullopt, std::move(callback),
        TryGetAuthTokensAndroidResult::kFailedDisabled);
    return;
  }

  auto quiche_proxy_layer = proxy_layer == ProxyLayer::kProxyA
                                ? quiche::ProxyLayer::kProxyA
                                : quiche::ProxyLayer::kProxyB;
  auto bsa_get_tokens_start_time = base::TimeTicks::Now();

  helper_.AsyncCall(&IpProtectionTokenFetcherHelper::GetTokensFromBlindSignAuth)
      .WithArgs(
          blind_sign_auth_.get(),
          quiche::BlindSignAuthServiceType::kChromeIpBlinding, std::nullopt,
          batch_size, quiche_proxy_layer,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &IpProtectionTokenIpcFetcher::OnFetchBlindSignedTokenCompleted,
              weak_ptr_factory_.GetWeakPtr(), bsa_get_tokens_start_time,
              std::move(callback))));
}

void IpProtectionTokenIpcFetcher::OnFetchBlindSignedTokenCompleted(
    base::TimeTicks bsa_get_tokens_start_time,
    TryGetAuthTokensCallback callback,
    absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens) {
  if (!tokens.ok()) {
    TryGetAuthTokensAndroidResult result;
    switch (tokens.status().code()) {
      case absl::StatusCode::kUnavailable:
        result = TryGetAuthTokensAndroidResult::kFailedBSATransient;
        break;
      case absl::StatusCode::kFailedPrecondition:
        result = TryGetAuthTokensAndroidResult::kFailedBSAPersistent;
        break;
      default:
        result = TryGetAuthTokensAndroidResult::kFailedBSAOther;
        break;
    }
    TryGetAuthTokensComplete(/*bsa_tokens=*/std::nullopt, std::move(callback),
                             result);
    return;
  }

  if (tokens.value().size() == 0) {
    TryGetAuthTokensComplete(
        /*bsa_tokens=*/std::nullopt, std::move(callback),
        TryGetAuthTokensAndroidResult::kFailedBSAOther);
    return;
  }

  std::optional<std::vector<ip_protection::BlindSignedAuthToken>> bsa_tokens =
      IpProtectionTokenFetcherHelper::QuicheTokensToIpProtectionAuthTokens(
          tokens.value());
  if (!bsa_tokens) {
    TryGetAuthTokensComplete(
        /*bsa_tokens=*/std::nullopt, std::move(callback),
        TryGetAuthTokensAndroidResult::kFailedBSAOther);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  TryGetAuthTokensComplete(std::move(bsa_tokens.value()), std::move(callback),
                           TryGetAuthTokensAndroidResult::kSuccess,
                           current_time - bsa_get_tokens_start_time);
}

void IpProtectionTokenIpcFetcher::TryGetAuthTokensComplete(
    std::optional<std::vector<ip_protection::BlindSignedAuthToken>> bsa_tokens,
    TryGetAuthTokensCallback callback,
    ip_protection::TryGetAuthTokensAndroidResult result,
    std::optional<base::TimeDelta> duration) {
  if (result == TryGetAuthTokensAndroidResult::kSuccess) {
    CHECK(bsa_tokens.has_value() && !bsa_tokens->empty());
  }

  ip_protection::Telemetry().AndroidTokenBatchFetchComplete(result, duration);

  std::optional<base::TimeDelta> backoff = CalculateBackoff(result);
  std::optional<base::Time> try_again_after;
  if (backoff) {
    if (*backoff == base::TimeDelta::Max()) {
      try_again_after = base::Time::Max();
    } else {
      try_again_after = base::Time::Now() + *backoff;
    }
  }
  DCHECK(bsa_tokens.has_value() || try_again_after.has_value());
  std::move(callback).Run(std::move(bsa_tokens), try_again_after);
}

std::optional<base::TimeDelta> IpProtectionTokenIpcFetcher::CalculateBackoff(
    TryGetAuthTokensAndroidResult result) {
  std::optional<base::TimeDelta> backoff;
  switch (result) {
    case TryGetAuthTokensAndroidResult::kSuccess:
      break;
    case TryGetAuthTokensAndroidResult::kFailedBSAPersistent:
    case TryGetAuthTokensAndroidResult::kFailedDisabled:
      backoff = base::TimeDelta::Max();
      break;
    case TryGetAuthTokensAndroidResult::kFailedBSATransient:
    case TryGetAuthTokensAndroidResult::kFailedBSAOther:
      backoff = net::features::kIpPrivacyTryGetAuthTokensTransientBackoff.Get();
      // Note that we calculate the backoff assuming that we've waited for
      // `last_try_get_auth_tokens_backoff_` time already, but this may not be
      // the case when:
      //  - Concurrent calls to `TryGetAuthTokens` from two network contexts are
      //  made and both fail in the same way
      //
      //  - The network service restarts (the new network context(s) won't know
      //  to backoff until after the first request(s))
      //
      // We can't do much about the first case, but for the others we could
      // track the backoff time here and not request tokens again until
      // afterward.
      if (last_try_get_auth_tokens_backoff_ &&
          last_try_get_auth_tokens_result_ == result) {
        backoff = *last_try_get_auth_tokens_backoff_ * 2;
      }
      break;
  }
  last_try_get_auth_tokens_result_ = result;
  last_try_get_auth_tokens_backoff_ = backoff;
  return backoff;
}

}  // namespace ip_protection
