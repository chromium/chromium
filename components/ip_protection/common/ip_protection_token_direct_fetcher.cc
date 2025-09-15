// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_direct_fetcher.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/builtin_categories.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "components/ip_protection/common/ip_protection_config_http.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "components/ip_protection/common/ip_protection_token_fetcher_helper.h"
#include "net/base/features.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ip_protection {

IpProtectionTokenDirectFetcher::SequenceBoundFetch::SequenceBoundFetch(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface>
        blind_sign_auth_for_testing) {
  CHECK(pending_url_loader_factory);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(pending_url_loader_factory));

  if (blind_sign_auth_for_testing) {
    blind_sign_auth_ = std::move(blind_sign_auth_for_testing);
    return;
  }
  privacy::ppn::BlindSignAuthOptions bsa_options{};
  bsa_options.set_enable_privacy_pass(true);

  blind_sign_auth_ = std::make_unique<quiche::BlindSignAuth>(
      std::make_unique<IpProtectionConfigHttp>(url_loader_factory_.get()),
      std::move(bsa_options));
}

IpProtectionTokenDirectFetcher::SequenceBoundFetch::~SequenceBoundFetch() =
    default;

void IpProtectionTokenDirectFetcher::SequenceBoundFetch::
    GetTokensFromBlindSignAuth(
        quiche::BlindSignAuthServiceType service_type,
        std::optional<std::string> access_token,
        uint32_t batch_size,
        quiche::ProxyLayer proxy_layer,
        perfetto::Track track,
        IpProtectionTokenFetcherHelper::FetchBlindSignedTokenCallback
            callback) {
  fetcher_helper_.GetTokensFromBlindSignAuth(
      blind_sign_auth_.get(), service_type, std::move(access_token), batch_size,
      proxy_layer, track, std::move(callback));
}

IpProtectionTokenDirectFetcher::IpProtectionTokenDirectFetcher(
    Delegate* delegate,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_for_testing)
    : delegate_(delegate),
      thread_pool_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      helper_(thread_pool_task_runner_,
              std::move(pending_url_loader_factory),
              std::move(blind_sign_auth_for_testing)) {}

IpProtectionTokenDirectFetcher::~IpProtectionTokenDirectFetcher() = default;

void IpProtectionTokenDirectFetcher::TryGetAuthTokens(
    uint32_t batch_size,
    ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  perfetto::NamedTrack parent("IpProtection");
  // Because the track doesn't get any events of its own it must manually emit
  // the track descriptor. SetTrackDescriptor may crash in unit tests where
  // tracing isn't initialized.
  if (perfetto::Tracing::IsInitialized()) {
    base::TrackEvent::SetTrackDescriptor(parent, parent.Serialize());
  } else {
    CHECK_IS_TEST();
  }
  perfetto::Track track(base::trace_event::GetNextGlobalTraceId(), parent);
  base::trace_event::EmitNamedTrigger("ipp-token-fetch-start");
  TRACE_EVENT_BEGIN("ip_protection", "TryGetAuthTokens", track);
  // If IP Protection is disabled via user settings then don't attempt to fetch
  // tokens.
  if (!delegate_->IsTokenFetchEnabled()) {
    TryGetAuthTokensComplete(std::nullopt, std::move(callback),
                             TryGetAuthTokensResult::kFailedDisabledByUser,
                             track);
    return;
  }

  // If we are in a state where the OAuth token has persistent errors then don't
  // try to request tokens.
  if (last_try_get_auth_tokens_backoff_ &&
      *last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    TryGetAuthTokensComplete(
        std::nullopt, std::move(callback),
        TryGetAuthTokensResult::kFailedOAuthTokenPersistent, track);
    return;
  }

  auto oauth_token_fetch_start_time = base::TimeTicks::Now();
  auto quiche_proxy_layer = proxy_layer == ProxyLayer::kProxyA
                                ? quiche::ProxyLayer::kProxyA
                                : quiche::ProxyLayer::kProxyB;
  auto request_token_callback = base::BindOnce(
      &IpProtectionTokenDirectFetcher::
          OnRequestOAuthTokenCompletedForTryGetAuthTokens,
      weak_ptr_factory_.GetWeakPtr(), batch_size, quiche_proxy_layer,
      std::move(callback), oauth_token_fetch_start_time, track);

  TRACE_EVENT_BEGIN("ip_protection", "RequestOAuthToken", track);
  delegate_->RequestOAuthToken(std::move(request_token_callback));
}

void IpProtectionTokenDirectFetcher::
    OnRequestOAuthTokenCompletedForTryGetAuthTokens(
        uint32_t batch_size,
        quiche::ProxyLayer quiche_proxy_layer,
        TryGetAuthTokensCallback callback,
        base::TimeTicks oauth_token_fetch_start_time,
        perfetto::Track track,
        TryGetAuthTokensResult result,
        std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_END("ip_protection", track);
  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (!access_token) {
    CHECK(result != TryGetAuthTokensResult::kSuccess);
    TryGetAuthTokensComplete(std::nullopt, std::move(callback), result, track);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  ip_protection::Telemetry().OAuthTokenFetchComplete(
      current_time - oauth_token_fetch_start_time);

  auto bsa_get_tokens_start_time = base::TimeTicks::Now();

  helper_
      .AsyncCall(&IpProtectionTokenDirectFetcher::SequenceBoundFetch::
                     GetTokensFromBlindSignAuth)
      .WithArgs(
          quiche::BlindSignAuthServiceType::kChromeIpBlinding,
          std::move(access_token), batch_size, quiche_proxy_layer, track,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &IpProtectionTokenDirectFetcher::OnFetchBlindSignedTokenCompleted,
              weak_ptr_factory_.GetWeakPtr(), bsa_get_tokens_start_time,
              std::move(callback), track)));
}

void IpProtectionTokenDirectFetcher::OnFetchBlindSignedTokenCompleted(
    base::TimeTicks bsa_get_tokens_start_time,
    TryGetAuthTokensCallback callback,
    perfetto::Track track,
    absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using enum TryGetAuthTokensResult;
  if (!tokens.ok()) {
    // Apply the canonical mapping from abseil status to HTTP status.
    TryGetAuthTokensResult result;
    switch (tokens.status().code()) {
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
    ip_protection::Telemetry().TryGetAuthTokensError(
        base::PersistentHash(tokens.status().ToString()));
    TryGetAuthTokensComplete(std::nullopt, std::move(callback), result, track);
    return;
  }

  if (tokens.value().size() == 0) {
    TryGetAuthTokensComplete(std::nullopt, std::move(callback), kFailedBSAOther,
                             track);
    return;
  }

  std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens =
      IpProtectionTokenFetcherHelper::QuicheTokensToIpProtectionAuthTokens(
          tokens.value());
  if (!bsa_tokens) {
    TryGetAuthTokensComplete(std::nullopt, std::move(callback), kFailedBSAOther,
                             track);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  TryGetAuthTokensComplete(std::move(bsa_tokens), std::move(callback), kSuccess,
                           track, current_time - bsa_get_tokens_start_time);
}

void IpProtectionTokenDirectFetcher::TryGetAuthTokensComplete(
    std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens,
    TryGetAuthTokensCallback callback,
    TryGetAuthTokensResult result,
    perfetto::Track track,
    std::optional<base::TimeDelta> duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_END("ip_protection", track);
  ip_protection::Telemetry().TokenBatchFetchComplete(result, duration);

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

void IpProtectionTokenDirectFetcher::AccountStatusChanged(
    bool account_available) {
  if (account_available) {
    // End the backoff period if it was caused by account-related issues.
    if (last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
      last_try_get_auth_tokens_backoff_.reset();
    }
  } else {
    last_try_get_auth_tokens_backoff_ = base::TimeDelta::Max();
  }
}

std::optional<base::TimeDelta> IpProtectionTokenDirectFetcher::CalculateBackoff(
    TryGetAuthTokensResult result) {
  using enum TryGetAuthTokensResult;
  std::optional<base::TimeDelta> backoff;
  bool exponential = false;
  switch (result) {
    case kSuccess:
      break;
    case kFailedNoAccount:
    case kFailedOAuthTokenPersistent:
    case kFailedDisabledByUser:
      backoff = base::TimeDelta::Max();
      break;
    case kFailedNotEligible:
    case kFailedBSA403:
      // Eligibility, whether determined locally or on the server, is unlikely
      // to change quickly.
      backoff =
          net::features::kIpPrivacyTryGetAuthTokensNotEligibleBackoff.Get();
      break;
    case kFailedOAuthTokenTransient:
    case kFailedBSAOther:
      // Transient failure to fetch an OAuth token, or some other error from
      // BSA that is probably transient.
      backoff = net::features::kIpPrivacyTryGetAuthTokensTransientBackoff.Get();
      exponential = true;
      break;
    case kFailedBSA400:
    case kFailedBSA401:
      // Both 400 and 401 suggest a bug, so do not retry aggressively.
      backoff = net::features::kIpPrivacyTryGetAuthTokensBugBackoff.Get();
      exponential = true;
      break;
    case kFailedOAuthTokenDeprecated:
      NOTREACHED();
  }

  // Note that we calculate the backoff assuming that we've waited for
  // `last_try_get_auth_tokens_backoff_` time already, but this may not be the
  // case when:
  //  - Concurrent calls to `TryGetAuthTokens` from two network contexts are
  //  made and both fail in the same way
  //
  //  - A new incognito window is opened (the new network context won't know
  //  to backoff until after the first request)
  //
  //  - The network service restarts (the new network context(s) won't know to
  //  backoff until after the first request(s))
  //
  // We can't do much about the first case, but for the others we could track
  // the backoff time here and not request tokens again until afterward.
  //
  // TODO(crbug.com/40280126): Track the backoff time in the browser
  // process and don't make new requests if we are in a backoff period.
  if (exponential) {
    if (last_try_get_auth_tokens_backoff_ &&
        last_try_get_auth_tokens_result_ == result) {
      backoff = *last_try_get_auth_tokens_backoff_ * 2;
    }
  }

  // If the backoff is due to a user account issue, then only update the
  // backoff time based on account status changes (via the login observer) and
  // not based on the result of any `TryGetAuthTokens()` calls.
  if (last_try_get_auth_tokens_backoff_ &&
      *last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    return *last_try_get_auth_tokens_backoff_;
  }

  last_try_get_auth_tokens_result_ = result;
  last_try_get_auth_tokens_backoff_ = backoff;

  if (backoff && backoff != base::TimeDelta::Max()) {
    return base::RandomizeByPercentage(
        *backoff, net::features::kIpPrivacyBackoffJitter.Get() * 100);
  }
  return backoff;
}

}  // namespace ip_protection
