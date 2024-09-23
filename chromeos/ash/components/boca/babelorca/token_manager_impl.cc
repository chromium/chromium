// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/token_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/token_fetcher.h"

namespace ash::babelorca {

TokenManagerImpl::TokenManagerImpl(std::unique_ptr<TokenFetcher> token_fetcher,
                                   base::TimeDelta expiration_buffer,
                                   base::Clock* clock)
    : token_fetcher_(std::move(token_fetcher)),
      expiration_buffer_(expiration_buffer),
      clock_(*clock) {}

TokenManagerImpl::~TokenManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const std::string* TokenManagerImpl::GetTokenString() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!token_data_ ||
      token_data_->expiration_time < clock_->Now() + expiration_buffer_) {
    return nullptr;
  }
  return &(token_data_->token);
}

int TokenManagerImpl::GetFetchedVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fetched_version_;
}

void TokenManagerImpl::ForceFetchToken(
    base::OnceCallback<void(bool)> success_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_requests_callbacks_.push(std::move(success_callback));
  if (pending_requests_callbacks_.size() > 1) {
    // Fetch request already in progress.
    return;
  }
  token_fetcher_->FetchToken(base::BindOnce(
      // base::Unretained is safe, `this` owns `token_fetcher_`.
      &TokenManagerImpl::OnTokenFetchCompleted, base::Unretained(this)));
}

void TokenManagerImpl::OnTokenFetchCompleted(
    std::optional<TokenDataWrapper> token_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool success = false;
  if (token_data) {
    token_data_ = std::move(token_data);
    ++fetched_version_;
    success = true;
  }
  while (!pending_requests_callbacks_.empty()) {
    std::move(pending_requests_callbacks_.front()).Run(success);
    pending_requests_callbacks_.pop();
  }
}

}  // namespace ash::babelorca
