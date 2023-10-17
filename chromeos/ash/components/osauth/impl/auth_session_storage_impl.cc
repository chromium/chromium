// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_session_storage_impl.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  auto token = base::UnguessableToken::Create().ToString();
  tokens_[token] =
      std::make_unique<AuthSessionStorageImpl::TokenData>(std::move(context));
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
    case TokenState::kOwned:
      return true;
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
  CHECK(data_it->second->state == TokenState::kOwned)
      << static_cast<int>(data_it->second->state);
  data_it->second->state = TokenState::kBorrowed;
  data_it->second->borrow_location = borrow_location;

  CHECK(data_it->second->context);
  return std::move(data_it->second->context);
}

void AuthSessionStorageImpl::BorrowAsync(const base::Location& location,
                                         const AuthProofToken& token,
                                         BorrowCallback callback) {
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
  data_it->second->borrow_queue.emplace(location, std::move(callback));
}

const UserContext* AuthSessionStorageImpl::Peek(const AuthProofToken& token) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  if (data_it->second->state == TokenState::kBorrowed) {
    LOG(ERROR) << "Context was already borrowed from "
               << data_it->second->borrow_location.ToString();
  }
  CHECK(data_it->second->state == TokenState::kOwned);
  CHECK(data_it->second->context);
  return data_it->second->context.get();
}

void AuthSessionStorageImpl::Return(const AuthProofToken& token,
                                    std::unique_ptr<UserContext> context) {
  CHECK(context);
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  CHECK(data_it->second->state == TokenState::kBorrowed);
  data_it->second->state = TokenState::kOwned;
  CHECK(!data_it->second->context);
  data_it->second->context = std::move(context);

  if (data_it->second->invalidate_on_return) {
    data_it->second->invalidate_on_return = false;
    Invalidate(token, std::move(data_it->second->invalidation_closure));
    return;
  }
  if (!data_it->second->borrow_queue.empty()) {
    std::pair<base::Location, BorrowCallback> pending_borrow =
        std::move(data_it->second->borrow_queue.front());
    data_it->second->borrow_queue.pop();
    BorrowAsync(pending_borrow.first, token, std::move(pending_borrow.second));
  }
}

void AuthSessionStorageImpl::Invalidate(const AuthProofToken& token,
                                        base::OnceClosure on_invalidated) {
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  if (data_it->second->state == TokenState::kBorrowed) {
    data_it->second->invalidate_on_return = true;
    data_it->second->invalidation_closure = std::move(on_invalidated);
    while (!data_it->second->borrow_queue.empty()) {
      std::pair<base::Location, BorrowCallback> pending_borrow =
          std::move(data_it->second->borrow_queue.front());
      data_it->second->borrow_queue.pop();
      std::move(pending_borrow.second).Run(nullptr);
    }
    return;
  }
  CHECK(data_it->second->state == TokenState::kOwned);
  data_it->second->state = TokenState::kInvalidating;
  auth_performer_->InvalidateAuthSession(
      std::move(data_it->second->context),
      base::BindOnce(&AuthSessionStorageImpl::OnSessionInvalidated,
                     weak_factory_.GetWeakPtr(), token,
                     std::move(on_invalidated)));
}

void AuthSessionStorageImpl::OnSessionInvalidated(
    const AuthProofToken& token,
    base::OnceClosure on_invalidated,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR)
        << "There was an error during attempt to invalidate auth session:"
        << error.value().get_cryptohome_code();
  };
  auto data_it = tokens_.find(token);
  CHECK(data_it != std::end(tokens_));
  CHECK(data_it->second->state == TokenState::kInvalidating);
  tokens_.erase(data_it);
  std::move(on_invalidated).Run();
}

}  // namespace ash
