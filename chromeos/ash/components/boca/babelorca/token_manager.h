// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/token_data_wrapper.h"

namespace babelorca {

class TokenFetcher;

// Class to fetch and store tokens.
class TokenManager {
 public:
  explicit TokenManager(std::unique_ptr<TokenFetcher> token_fetcher,
                        base::TimeDelta expiration_buffer = base::Seconds(5),
                        base::Clock* clock = base::DefaultClock::GetInstance());

  TokenManager(const TokenManager&) = delete;
  TokenManager& operator=(const TokenManager&) = delete;

  ~TokenManager();

  // Returns pointer to the token string if it has been fetched and did not
  // expire, otherwise returns null. Returned string pointer should not be
  // stored.
  const std::string* GetTokenString();

  // Gets the version of the existing fetched token, 0 means no token has been
  // fetched. Useful to identify if the token has changed since the last call.
  int GetFetchedVersion();

  // Fetches and stores the token, existing token will be overwritten and
  // `success_callback` will be called with `true` if fetch was successful. If
  // fetch fails, `success_callback` will be called with `false` and existing
  // token will not be overwritten. If fetch is already in progress,
  // `success_callback` will be queued and called when fetch is complete.
  void ForceFetchToken(base::OnceCallback<void(bool)> success_callback);

 private:
  void OnTokenFetchCompleted(std::optional<TokenDataWrapper> token_data);

  const std::unique_ptr<TokenFetcher> token_fetcher_;
  const base::TimeDelta expiration_buffer_;
  const raw_ref<const base::Clock> clock_;

  SEQUENCE_CHECKER(sequence_checker_);
  size_t fetched_version_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  std::optional<TokenDataWrapper> token_data_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::queue<base::OnceCallback<void(bool)>> pending_requests_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_H_
