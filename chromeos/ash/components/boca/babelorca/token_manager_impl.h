// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_IMPL_H_

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
#include "chromeos/ash/components/boca/babelorca/token_manager.h"

namespace ash::babelorca {

class TokenFetcher;

// Class to fetch and store tokens.
class TokenManagerImpl : public TokenManager {
 public:
  explicit TokenManagerImpl(
      std::unique_ptr<TokenFetcher> token_fetcher,
      base::TimeDelta expiration_buffer = base::Seconds(5),
      base::Clock* clock = base::DefaultClock::GetInstance());

  TokenManagerImpl(const TokenManagerImpl&) = delete;
  TokenManagerImpl& operator=(const TokenManagerImpl&) = delete;

  ~TokenManagerImpl() override;

  // TokenManager:
  const std::string* GetTokenString() override;
  int GetFetchedVersion() override;
  void ForceFetchToken(
      base::OnceCallback<void(bool)> success_callback) override;

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

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_MANAGER_IMPL_H_
