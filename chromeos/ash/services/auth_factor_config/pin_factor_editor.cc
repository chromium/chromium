// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/pin_factor_editor.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

PinFactorEditor::PinFactorEditor(AuthFactorConfig* auth_factor_config,
                                 PinBackendDelegate* pin_backend,
                                 QuickUnlockStorageDelegate* storage)
    : auth_factor_config_(auth_factor_config),
      pin_backend_(pin_backend),
      quick_unlock_storage_(storage),
      auth_factor_editor_(UserDataAuthClient::Get()) {
  CHECK(auth_factor_config_);
  CHECK(pin_backend_);
  CHECK(quick_unlock_storage_);
}

PinFactorEditor::~PinFactorEditor() = default;

void PinFactorEditor::SetPin(
    const std::string& auth_token,
    const std::string& pin,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  AccountId account_id;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }
    account_id =
        ash::AuthSessionStorage::Get()->Peek(auth_token)->GetAccountId();
  } else {
    const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
    CHECK(user);
    auto* user_context_ptr =
        quick_unlock_storage_->GetUserContext(user, auth_token);
    if (!user_context_ptr) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }
    account_id = user_context_ptr->GetAccountId();
  }
  pin_backend_->Set(account_id, auth_token, pin,
                    base::BindOnce(&PinFactorEditor::OnPinConfigured,
                                   weak_factory_.GetWeakPtr(), auth_token,
                                   std::move(callback)));
}

void PinFactorEditor::RemovePin(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  AccountId account_id;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }
    account_id =
        ash::AuthSessionStorage::Get()->Peek(auth_token)->GetAccountId();
  } else {
    const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
    CHECK(user);
    auto* user_context_ptr =
        quick_unlock_storage_->GetUserContext(user, auth_token);
    if (user_context_ptr == nullptr) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }
    account_id = user_context_ptr->GetAccountId();
  }
  auth_factor_config_->IsConfigured(
      auth_token, mojom::AuthFactor::kPin,
      base::BindOnce(&PinFactorEditor::OnIsPinConfiguredForRemove,
                     weak_factory_.GetWeakPtr(), account_id, auth_token,
                     std::move(callback)));
}

void PinFactorEditor::OnIsPinConfiguredForRemove(
    const AccountId account_id,
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool is_pin_configured) {
  if (!is_pin_configured) {
    LOG(WARNING)
        << "No PIN configured, ignoring PinFactorEditor::RemovePin call";
    std::unique_ptr<UserContext> context;
    if (ash::features::ShouldUseAuthSessionStorage()) {
      context = ash::AuthSessionStorage::Get()->Borrow(FROM_HERE, auth_token);
    } else {
      const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
      CHECK(user);
      auto* user_context_ptr =
          quick_unlock_storage_->GetUserContext(user, auth_token);
      CHECK(user_context_ptr);
      context = std::make_unique<UserContext>(*user_context_ptr);
    }
    if (ash::features::ShouldUseAuthSessionStorage()) {
      ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
    }
    std::move(callback).Run(mojom::ConfigureResult::kSuccess);
    return;
  }
  pin_backend_->Remove(account_id, auth_token,
                       base::BindOnce(&PinFactorEditor::OnPinConfigured,
                                      weak_factory_.GetWeakPtr(), auth_token,
                                      std::move(callback)));
}

void PinFactorEditor::OnPinConfigured(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success) {
  std::unique_ptr<UserContext> context;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    context = ash::AuthSessionStorage::Get()->Borrow(FROM_HERE, auth_token);
  } else {
    const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
    CHECK(user);
    auto* user_context_ptr =
        quick_unlock_storage_->GetUserContext(user, auth_token);
    CHECK(user_context_ptr);
    context = std::make_unique<UserContext>(*user_context_ptr);
  }

  if (!success) {
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {mojom::AuthFactor::kPin}, auth_token, std::move(context),
      std::move(callback));
}

void PinFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::PinFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace ash::auth
