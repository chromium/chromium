// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_session_storage_impl.h"

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthSessionStorageImpl::AuthSessionStorageImpl(
    UserDataAuthClient* user_data_auth,
    const base::Clock* clock)
    : clock_(clock) {
  auth_performer_ = std::make_unique<AuthPerformer>(user_data_auth, clock_);
}

AuthSessionStorageImpl::~AuthSessionStorageImpl() = default;

AuthSessionStorageImpl::TokenData::TokenData(
    std::unique_ptr<UserContext> context)
    : context(std::move(context)) {}
AuthSessionStorageImpl::TokenData::~TokenData() = default;

AuthProofToken AuthSessionStorageImpl::Store(
    std::unique_ptr<UserContext> context) {
  CHECK(context);
  CHECK(!context->GetSessionLifetime().is_null());
  auto token = base::UnguessableToken::Create().ToString();
  tokens_[token] =
      std::make_unique<AuthSessionStorageImpl::TokenData>(std::move(context));
  HandleSessionRefresh(token);
  return token;
}

bool AuthSessionStorageImpl::IsValid(const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  if (data_it == std::end(tokens_)) {
    return false;
  }
  switch (data_it->second->state) {
    case TokenState::kBorrowed:
      return !data_it->second->invalidate_on_return;
    case TokenState::kOwned: {
      base::Time lifetime = data_it->second->context->GetSessionLifetime();
      // Edge case: non-authenticated session. Consider it invalid.
      if (lifetime.is_null()) {
        return false;
      }
      base::TimeDelta remaining_lifetime = lifetime - clock_->Now();
      return remaining_lifetime.is_positive();
    }
    case TokenState::kInvalidating:
      return false;
  }
}

std::unique_ptr<UserContext> AuthSessionStorageImpl::BorrowForTests(
    const base::Location& borrow_location,
    const AuthProofToken& token) {
  return Borrow(borrow_location, token);
}

std::unique_ptr<UserContext> AuthSessionStorageImpl::Borrow(
    const base::Location& borrow_location,
    const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  if (data_it->second->state == TokenState::kBorrowed) {
    LOG(ERROR) << "Context was already borrowed from "
               << data_it->second->borrow_location.ToString()
               << " when trying to borrow from " << borrow_location.ToString();
  }

  CHECK_EQ(data_it->second->state, TokenState::kOwned);
  data_it->second->state = TokenState::kBorrowed;
  data_it->second->borrow_location = borrow_location;

  CHECK(data_it->second->context);
  return std::move(data_it->second->context);
}

void AuthSessionStorageImpl::BorrowAsync(const base::Location& location,
                                         const AuthProofToken& token,
                                         BorrowContextCallback callback) {
  auto data_it = tokens_.find(token);
  if (data_it == std::end(tokens_)) {
    LOG(ERROR) << "Accessing expired token";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  if (data_it->second->state == TokenState::kOwned) {
    auto context = Borrow(location, token);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(context)));
    return;
  }
  if (data_it->second->state == TokenState::kInvalidating ||
      data_it->second->invalidate_on_return) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  if (data_it->second->withdraw_callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  data_it->second->borrow_queue.emplace(location, std::move(callback));
}

const UserContext* AuthSessionStorageImpl::Peek(const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  if (data_it->second->state == TokenState::kBorrowed) {
    LOG(ERROR) << "Context was already borrowed from "
               << data_it->second->borrow_location.ToString();
  }
  CHECK_EQ(data_it->second->state, TokenState::kOwned);
  CHECK(data_it->second->context);
  return data_it->second->context.get();
}

void AuthSessionStorageImpl::Return(const AuthProofToken& token,
                                    std::unique_ptr<UserContext> context) {
  CHECK(context);
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  CHECK_EQ(data_it->second->state, TokenState::kBorrowed);
  data_it->second->state = TokenState::kOwned;
  CHECK(!data_it->second->context);
  data_it->second->context = std::move(context);

  if (data_it->second->invalidate_on_return) {
    data_it->second->invalidate_on_return = false;
    Invalidate(token, std::nullopt);
    return;
  }

  if (data_it->second->withdraw_callback) {
    CHECK(data_it->second->context);
    auto stored_context = std::move(data_it->second->context);
    auto callback = std::move(data_it->second->withdraw_callback.value());
    // Invalidation queue should be handled by condition above,
    // and borrow queue should be empty per invariant
    // in BorrowAsync/Withdraw methods.
    CHECK(data_it->second->borrow_queue.empty());
    CHECK(data_it->second->invalidation_queue.empty());
    tokens_.erase(data_it);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(stored_context)));
    return;
  }

  if (data_it->second->keep_alive_counter > 0) {
    HandleSessionRefresh(token);
    auto check_still_alive = tokens_.find(token);
    if (check_still_alive == std::end(tokens_)) {
      // Session was invalidated as it was returned
      // too late.
      return;
    }
    if (data_it->second->state != TokenState::kOwned) {
      return;
    }
  }

  if (!data_it->second->borrow_queue.empty()) {
    std::pair<base::Location, BorrowContextCallback> pending_borrow =
        std::move(data_it->second->borrow_queue.front());
    data_it->second->borrow_queue.pop();
    BorrowAsync(pending_borrow.first, token, std::move(pending_borrow.second));
  }
}

void AuthSessionStorageImpl::Withdraw(const AuthProofToken& token,
                                      BorrowContextCallback callback) {
  auto data_it = tokens_.find(token);
  if (data_it == std::end(tokens_)) {
    LOG(ERROR) << "Accessing expired token";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  if (data_it->second->state == TokenState::kOwned) {
    CHECK(data_it->second->context);
    auto context = std::move(data_it->second->context);
    // As context is owned, there should be no waiting borrow/invalidate
    // callbacks.
    CHECK(data_it->second->borrow_queue.empty());
    CHECK(data_it->second->invalidation_queue.empty());
    tokens_.erase(data_it);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(context)));
    return;
  }

  if (data_it->second->state == TokenState::kInvalidating ||
      data_it->second->invalidate_on_return) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  CHECK_EQ(data_it->second->state, TokenState::kBorrowed);
  // Drain borrow queue.
  while (!data_it->second->borrow_queue.empty()) {
    std::pair<base::Location, BorrowContextCallback> pending_borrow =
        std::move(data_it->second->borrow_queue.front());
    data_it->second->borrow_queue.pop();
    std::move(pending_borrow.second).Run(nullptr);
  }

  CHECK(!data_it->second->withdraw_callback) << "There can be only one!";
  data_it->second->withdraw_callback = std::move(callback);
}

void AuthSessionStorageImpl::Invalidate(
    const AuthProofToken& token,
    std::optional<InvalidationCallback> on_invalidated) {
  auto data_it = tokens_.find(token);
  // If token was already invalidated, just call a callback.
  if (data_it == std::end(tokens_)) {
    if (on_invalidated) {
      std::move(*on_invalidated).Run();
    }
    return;
  }

  if (on_invalidated) {
    data_it->second->invalidation_queue.push(std::move(*on_invalidated));
  }
  // Drain borrow queue.
  while (!data_it->second->borrow_queue.empty()) {
    std::pair<base::Location, BorrowContextCallback> pending_borrow =
        std::move(data_it->second->borrow_queue.front());
    data_it->second->borrow_queue.pop();
    std::move(pending_borrow.second).Run(nullptr);
  }

  if (data_it->second->state == TokenState::kBorrowed) {
    data_it->second->invalidate_on_return = true;
    return;
  }
  if (data_it->second->state == TokenState::kInvalidating) {
    return;
  }

  CHECK_EQ(data_it->second->state, TokenState::kOwned);

  data_it->second->state = TokenState::kInvalidating;
  data_it->second->next_action_timer_.reset();

  auth_performer_->InvalidateAuthSession(
      std::move(data_it->second->context),
      base::BindOnce(&AuthSessionStorageImpl::OnSessionInvalidated,
                     weak_factory_.GetWeakPtr(), token));
}

std::unique_ptr<ScopedSessionRefresher> AuthSessionStorageImpl::KeepAlive(
    const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  // Not using make_unique due to private constructor.
  return base::WrapUnique(
      new ScopedSessionRefresherImpl(weak_factory_.GetWeakPtr(), token));
}

bool AuthSessionStorageImpl::CheckHasKeepAliveForTesting(
    const AuthProofToken& token) const {
  auto data_it = tokens_.find(token);
  return data_it == std::end(tokens_)
             ? false
             : data_it->second->keep_alive_counter >= 1;
}

void AuthSessionStorageImpl::OnSessionInvalidated(
    const AuthProofToken& token,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR)
        << "There was an error during attempt to invalidate auth session:"
        << error.value().get_cryptohome_code();
  };
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  CHECK_EQ(data_it->second->state, TokenState::kInvalidating);
  std::queue<InvalidationCallback> invalidation_queue =
      std::move(data_it->second->invalidation_queue);
  tokens_.erase(data_it);
  while (!invalidation_queue.empty()) {
    InvalidationCallback callback = std::move(invalidation_queue.front());
    invalidation_queue.pop();
    std::move(callback).Run();
  }
}

void AuthSessionStorageImpl::IncreaseKeepAliveCounter(
    const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  data_it->second->keep_alive_counter++;
  if (data_it->second->keep_alive_counter == 1) {
    HandleSessionRefresh(token);
  }
}

void AuthSessionStorageImpl::HandleSessionRefresh(const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  if (data_it->second->state != TokenState::kOwned) {
    // Retry once we have context.
    return;
  }
  base::Time valid_until = data_it->second->context->GetSessionLifetime();
  if (valid_until.is_null()) {
    // Non-authenticated session.
    LOG(WARNING) << "Non-authenticated session in AuthSessionStorage";
    return;
  }
  base::TimeDelta remaining_lifetime = valid_until - clock_->Now();
  if (remaining_lifetime.is_negative()) {
    // Too late.
    LOG(ERROR) << "Could not extend authsession lifetime before it timed out.";
    Invalidate(token, std::nullopt);
    return;
  }
  if (data_it->second->keep_alive_counter <= 0) {
    // No need to keep session alive, set timer to invalidate session.
    data_it->second->next_action_timer_ =
        std::make_unique<base::OneShotTimer>();
    data_it->second->next_action_timer_->Start(
        FROM_HERE, remaining_lifetime,
        base::BindOnce(&AuthSessionStorageImpl::HandleSessionRefresh,
                       weak_factory_.GetWeakPtr(), token));
    return;
  }
  if (remaining_lifetime <= cryptohome::kAuthsessionExtendThreshold) {
    // Trigger extension immediately.
    ExtendAuthSession(token);
    return;
  }
  base::TimeDelta refresh_after =
      remaining_lifetime - cryptohome::kAuthsessionExtendThreshold;
  if (refresh_after.is_negative()) {
    return;
  }
  data_it->second->next_action_timer_ = std::make_unique<base::OneShotTimer>();
  data_it->second->next_action_timer_->Start(
      FROM_HERE, refresh_after,
      base::BindOnce(&AuthSessionStorageImpl::HandleSessionRefresh,
                     weak_factory_.GetWeakPtr(), token));
}

void AuthSessionStorageImpl::ExtendAuthSession(const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  CHECK_EQ(data_it->second->state, TokenState::kOwned);
  CHECK_GT(data_it->second->keep_alive_counter, 0);
  auth_performer_->ExtendAuthSessionLifetime(
      Borrow(FROM_HERE, token),
      base::BindOnce(&AuthSessionStorageImpl::OnExtendAuthSession,
                     weak_factory_.GetWeakPtr(), token));
}

void AuthSessionStorageImpl::OnExtendAuthSession(
    const AuthProofToken& token,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR)
        << "There was an error during attempt to extend auth session lifetime "
        << error.value().get_cryptohome_code();
  }
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  CHECK_EQ(data_it->second->state, TokenState::kBorrowed);

  data_it->second->context = std::move(context);
  data_it->second->state = TokenState::kOwned;

  if (data_it->second->invalidate_on_return) {
    data_it->second->invalidate_on_return = false;
    Invalidate(token, std::nullopt);
    return;
  }
  // Schedule next refresh if needed.
  if (!error.has_value()) {
    HandleSessionRefresh(token);
  }
}

void AuthSessionStorageImpl::DecreaseKeepAliveCounter(
    const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  if (data_it == std::end(tokens_)) {
    // Token could be explicitly invalidated by now.
    return;
  }
  data_it->second->keep_alive_counter--;
  if (data_it->second->keep_alive_counter == 0) {
    data_it->second->next_action_timer_.reset();
    // Maybe start invalidation timer.
    HandleSessionRefresh(token);
  }
}

ScopedSessionRefresherImpl::ScopedSessionRefresherImpl(
    base::WeakPtr<AuthSessionStorageImpl> storage,
    const AuthProofToken& token)
    : storage_(std::move(storage)), token_(token) {
  storage_->IncreaseKeepAliveCounter(token_);
}

ScopedSessionRefresherImpl::~ScopedSessionRefresherImpl() {
  if (storage_) {
    storage_->DecreaseKeepAliveCounter(token_);
  }
}

}  // namespace ash
