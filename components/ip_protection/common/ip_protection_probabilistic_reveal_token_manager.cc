// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_manager.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"

namespace ip_protection {

IpProtectionProbabilisticRevealTokenManager::
    IpProtectionProbabilisticRevealTokenManager(
        std::unique_ptr<IpProtectionProbabilisticRevealTokenFetcher> fetcher)
    : fetcher_(std::move(fetcher)), expiration_(base::Time::UnixEpoch()) {
  DCHECK(fetcher_);
  RequestTokens();
}

IpProtectionProbabilisticRevealTokenManager::
    ~IpProtectionProbabilisticRevealTokenManager() {
  refetch_timer_.Stop();
}

void IpProtectionProbabilisticRevealTokenManager::RequestTokens() {
  if (!fetcher_) {
    // This should not happen in theory, caller should not hand
    // manager a null fetcher.
    return;
  }
  fetcher_->TryGetProbabilisticRevealTokens(base::BindOnce(
      &IpProtectionProbabilisticRevealTokenManager::OnTryGetTokens,
      weak_ptr_factory_.GetWeakPtr()));
}

void IpProtectionProbabilisticRevealTokenManager::OnTryGetTokens(
    std::optional<TryGetProbabilisticRevealTokensOutcome> outcome,
    TryGetProbabilisticRevealTokensResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.try_again_after.has_value()) {
    // Network error when fetching, re-try at the time fetcher asked. Fetcher
    // implements exponential backoff.
    refetch_timer_.Start(
        FROM_HERE, result.try_again_after.value() - base::Time::Now(), this,
        &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
    return;
  } else if (result.status != TryGetProbabilisticRevealTokensStatus::kSuccess) {
    // Response returned from server, however it is invalid.
    // This might happen if the PRT issuer server is configured wrong.
    // Fetcher did not specify when to retry. Retry in an hour.
    refetch_timer_.Start(
        FROM_HERE, base::Hours(1), this,
        &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
    return;
  }
  DCHECK(outcome.has_value());
  auto maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
      outcome.value().public_key, outcome.value().tokens);
  if (!maybe_crypter.ok()) {
    // Might happen if PRT issuer is misconfigured and public_key or tokens do
    // not belong to the group. Return without clearing existing tokens, which
    // might be still valid for current epoch. Retry in an hour.
    refetch_timer_.Start(
        FROM_HERE, base::Hours(1), this,
        &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
    return;
  }
  crypter_ = std::move(maybe_crypter.value());
  token_map_.clear();
  expiration_ = base::Time::FromMillisecondsSinceUnixEpoch(
      base::Seconds(outcome.value().expiration_time_seconds).InMilliseconds());
  num_tokens_with_signal_ = outcome.value().num_tokens_with_signal;

  auto next_request_delta =
      base::Time::FromMillisecondsSinceUnixEpoch(
          base::Seconds(outcome.value().next_epoch_start_time_seconds)
              .InMilliseconds()) -
      base::Time::Now();
  if (next_request_delta.is_negative()) {
    // Either client time is wrong or PRT issuer server returned wrong
    // next_epoch_start, most likely client time. Schedule next request
    // in three hours.
    next_request_delta = base::Hours(3);
  }
  refetch_timer_.Start(
      FROM_HERE, next_request_delta, this,
      &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
  // TODO(crbug.com/391358904): add metrics
}

bool IpProtectionProbabilisticRevealTokenManager::AreTokensExpired() const {
  return (base::Time::Now() >= expiration_);
}

void IpProtectionProbabilisticRevealTokenManager::ClearTokens() {
  token_map_.clear();
  if (crypter_) {
    crypter_->ClearTokens();
  }
}

void IpProtectionProbabilisticRevealTokenManager::ClearTokensIfExpired() {
  if (AreTokensExpired()) {
    ClearTokens();
  }
}

bool IpProtectionProbabilisticRevealTokenManager::IsTokenAvailable() {
  ClearTokensIfExpired();
  return crypter_ && crypter_->IsTokenAvailable();
}

std::optional<ProbabilisticRevealToken>
IpProtectionProbabilisticRevealTokenManager::GetToken(
    const std::string& top_level,
    const std::string& third_party) {
  ClearTokensIfExpired();
  if (!IsTokenAvailable()) {
    return std::nullopt;
  }
  // Manager has tokens, crypter_ is not null from here on and
  // crypter_->NumTokens() > 0 holds.
  auto outer_iterator = token_map_.find(top_level);
  if (outer_iterator != token_map_.end()) {
    // First party already has an associated token.
    const std::size_t token_index = outer_iterator->second.first;
    auto& inner_map = outer_iterator->second.second;

    const auto inner_iterator = inner_map.find(third_party);
    if (inner_iterator != inner_map.end()) {
      // The pair already has a randomized token.
      return inner_iterator->second;
    }

    // Seeing this third party for the first time in this top level.
    // Randomize top level's token and return.
    const auto maybe_randomized_token = crypter_->Randomize(token_index);
    if (!maybe_randomized_token.ok()) {
      // Should not happen in theory, might happen with corrupted crypter.
      return std::nullopt;
    }
    inner_map[third_party] = std::move(maybe_randomized_token.value());
    return inner_map[third_party];
  }
  // Seeing first party for the first time.
  const std::size_t& token_selected =
      base::RandGenerator(crypter_->NumTokens());
  const auto maybe_randomized_token = crypter_->Randomize(token_selected);
  if (!maybe_randomized_token.ok()) {
    // Should not happen in theory, might happen with corrupted crypter.
    return std::nullopt;
  }
  token_map_[top_level] = {token_selected,
                           {{third_party, maybe_randomized_token.value()}}};
  return std::move(maybe_randomized_token.value());
}

}  // namespace ip_protection
