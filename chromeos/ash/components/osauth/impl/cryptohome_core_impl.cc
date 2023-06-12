// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/cryptohome_core_impl.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

AuthSessionIntent MapPurposeToIntent(AuthPurpose purpose) {
  switch (purpose) {
    case AuthPurpose::kLogin:
    case AuthPurpose::kAuthSettings:
      return AuthSessionIntent::kDecrypt;
    case AuthPurpose::kWebAuthN:
      return AuthSessionIntent::kWebAuthn;
    case AuthPurpose::kUserVerification:
    case AuthPurpose::kScreenUnlock:
      return AuthSessionIntent::kVerifyOnly;
  }
}

}  // namespace

CryptohomeCoreImpl::CryptohomeCoreImpl(UserDataAuthClient* client)
    : dbus_client_(client) {}

CryptohomeCoreImpl::~CryptohomeCoreImpl() = default;

void CryptohomeCoreImpl::WaitForService(ServiceAvailabilityCallback callback) {
  dbus_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&CryptohomeCoreImpl::OnServiceStatus,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CryptohomeCoreImpl::OnServiceStatus(ServiceAvailabilityCallback callback,
                                         bool service_is_available) {
  std::move(callback).Run(service_is_available);
}

void CryptohomeCoreImpl::StartAuthSession(const AuthAttemptVector& attempt,
                                          Client* client) {
  if (current_attempt_.has_value()) {
    CHECK(attempt == *current_attempt_)
        << "Cryptohome core does not support parallel attempts";
  } else {
    current_attempt_ = attempt;
    is_authorized_ = false;
    performer_->InvalidateCurrentAttempts();
  }
  DCHECK(!clients_.contains(client));
  clients_.insert(client);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(attempt.account);
  CHECK(user) << "Cryptohome core should only be used for existing users";
  context_ = std::make_unique<UserContext>(user->GetType(), attempt.account);
  bool ephemeral = user_manager::UserManager::Get()->IsEphemeralUser(user);

  performer_->StartAuthSession(
      std::move(context_), ephemeral, MapPurposeToIntent(attempt.purpose),
      base::BindOnce(&CryptohomeCoreImpl::OnAuthSessionStarted,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomeCoreImpl::OnAuthSessionStarted(
    bool user_exists,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (!user_exists) {
    // Somehow user home directory does not exist.
    LOG(ERROR) << "Cryptohome Core: user does not exist";
    for (auto& client : clients_) {
      client->OnAuthSessionStartFailure();
    }
    clients_.clear();
    return;
  }

  if (error.has_value()) {
    // Error is already logged by Authenticator.
    for (auto& client : clients_) {
      client->OnAuthSessionStartFailure();
    }
    clients_.clear();
    return;
  }

  context_ = std::move(context);

  for (auto& client : clients_) {
    client->OnCryptohomeAuthSessionStarted();
  }
}

void CryptohomeCoreImpl::EndAuthSession(Client* client) {
  DCHECK(clients_.contains(client));
  DCHECK(!clients_being_removed_.contains(client));
  clients_.erase(client);
  clients_being_removed_.insert(client);
  if (!clients_.empty()) {
    // Wait for all clients to issue EndAuthSession.
    return;
  }
  if (context_) {
    performer_->InvalidateAuthSession(
        std::move(context_),
        base::BindOnce(&CryptohomeCoreImpl::OnInvalidateAuthSession,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  // We should have no context only when session is authorized and
  // one of the clients requested `StoreAuthenticatedContext`.
  CHECK(is_authorized_);
  EndAuthSessionImpl();
}

void CryptohomeCoreImpl::OnInvalidateAuthSession(
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Error during authsession invalidation";
  }
  EndAuthSessionImpl();
}

void CryptohomeCoreImpl::EndAuthSessionImpl() {
  for (auto& client : clients_being_removed_) {
    client->OnCryptohomeAuthSessionFinished();
  }
  clients_being_removed_.clear();
  CHECK(clients_.empty());
  current_attempt_ = absl::nullopt;
  is_authorized_ = false;
}

AuthPerformer* CryptohomeCoreImpl::GetAuthPerformer() const {
  CHECK(performer_);
  return performer_.get();
}

UserContext* CryptohomeCoreImpl::GetCurrentContext() const {
  CHECK(context_);
  return context_.get();
}

AuthProofToken CryptohomeCoreImpl::StoreAuthenticationContext() {
  CHECK(context_);
  return AuthSessionStorage::Get()->Store(std::move(context_));
}

std::unique_ptr<UserContext> CryptohomeCoreImpl::BorrowContext() {
  CHECK(context_);
  return std::move(context_);
}

void CryptohomeCoreImpl::ReturnContext(std::unique_ptr<UserContext> context) {
  CHECK(!context_);
  context_ = std::move(context);
}

}  // namespace ash
