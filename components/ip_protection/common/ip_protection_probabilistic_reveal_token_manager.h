// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_MANAGER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_MANAGER_H_

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ip_protection {

class IpProtectionProbabilisticRevealTokenCrypter;
class IpProtectionProbabilisticRevealTokenFetcher;
class IpProtectionProbabilisticRevealTokenDataStorage;
struct ProbabilisticRevealToken;
struct TryGetProbabilisticRevealTokensResult;
struct TryGetProbabilisticRevealTokensOutcome;

// IpProtectionProbabilisticRevealTokenManager uses fetcher to fetch and crypter
// to store, randomize and retrieve probabilistic reveal tokens (PRT). It
// requests a new batch under necessary conditions.
class IpProtectionProbabilisticRevealTokenManager {
 public:
  // Constructs manager and tries to fetch tokens immediately (async).
  IpProtectionProbabilisticRevealTokenManager(
      std::unique_ptr<IpProtectionProbabilisticRevealTokenFetcher> fetcher,
      std::optional<base::FilePath> data_directory);
  virtual ~IpProtectionProbabilisticRevealTokenManager();

  // Returns true if there are tokens in cache.
  virtual bool IsTokenAvailable();

  // Get a PRT for a given first and third party pair.
  // `top_level` and `third_party` are eTLD + 1.
  //
  // * If crypter_ is null or does not have tokens return null.
  // * If there is already a token associated for the first and
  //   third party pair in `token_map_` return it.
  // * If there is a token associated with first party, that means
  //   we are seeing this third party for the first time, randomize
  //   the token associated with first party and return it.
  // * Else, seeing the first party for the first time, pick a
  //   token stored in crypter randomly, and randomize it,
  //   and return it.
  virtual std::optional<std::string> GetToken(
      const std::string& top_level,
      const std::string& third_party);

  // Request new batch of tokens.
  virtual void RequestTokens();

 private:
  // Passed to fetcher as a callback. Stores token fetch result in internal
  // members and schedules next request using `refetch_timer_`.
  void OnTryGetTokens(
      std::optional<TryGetProbabilisticRevealTokensOutcome> outcome,
      TryGetProbabilisticRevealTokensResult result);

  // Return true if current batch of tokens are expired.
  bool AreTokensExpired() const;

  // Clears both `crypter_` and `token_map_`.
  void ClearTokens();

  // Calls `ClearTokens()` if current batch is expired.
  void ClearTokensIfExpired();

  // Stores the outcome of a token fetch if the feature is enabled.
  void StoreTokenOutcomeIfEnabled(
      TryGetProbabilisticRevealTokensOutcome outcome);

  // Serializes the given token.
  std::optional<std::string> SerializePrt(ProbabilisticRevealToken token);

  // True the first time GetToken() is called, false otherwise.
  bool is_initial_get_token_call_ = true;

  // Time that the current token fetch began, for measurement.
  base::TimeTicks token_fetch_start_time_;

  // Used for fetching tokens.
  std::unique_ptr<IpProtectionProbabilisticRevealTokenFetcher> fetcher_;

  // Tokens are invalid past `expiration_` timestamp.
  base::Time expiration_;

  // Number of tokens in the current batch that has the signal.
  int32_t num_tokens_with_signal_ = 0;

  // Epoch id of the current batch of tokens.
  std::string epoch_id_;

  // Map for tokens associated with a first and third party pair.
  // Keys are first party eTLD+1, and values are pairs of,
  // - index of the token (in crypter) for the first party for the
  //   current epoch,
  // - maps, where keys are third party eTLD+1 and values are
  //   the randomized version of the token at index (in crypter) for the
  //   first/third party pair for the current epoch.
  std::map<std::string,
           std::pair<std::size_t, /*index of token in crypter*/
                     std::map<std::string, ProbabilisticRevealToken>>>
      token_map_;

  // Stores tokens. Provides a method to randomize and retrieve a token at a
  // given index.
  std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter> crypter_;

  // Data storage for persisting tokens when the feature is enabled.
  base::SequenceBound<IpProtectionProbabilisticRevealTokenDataStorage> storage_;

  // A timer to schedule the next token batch request.
  base::OneShotTimer refetch_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionProbabilisticRevealTokenManager>
      weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_MANAGER_H_
