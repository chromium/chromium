// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/pin_factor_editor.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
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
  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(user);
  auto* user_context_ptr =
      quick_unlock_storage_->GetUserContext(user, auth_token);
  if (user_context_ptr == nullptr) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  auto context = std::make_unique<UserContext>(*user_context_ptr);

  pin_backend_->Set(user->GetAccountId(), auth_token, pin,
                    base::BindOnce(&PinFactorEditor::OnPinConfigured,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(context), std::move(callback)));
}

void PinFactorEditor::RemovePin(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(user);
  auto* user_context_ptr =
      quick_unlock_storage_->GetUserContext(user, auth_token);
  if (user_context_ptr == nullptr) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  auto context = std::make_unique<UserContext>(*user_context_ptr);

  auth_factor_config_->IsConfigured(
      auth_token, mojom::AuthFactor::kPin,
      base::BindOnce(&PinFactorEditor::OnIsPinConfiguredForRemove,
                     weak_factory_.GetWeakPtr(), auth_token, std::move(context),
                     std::move(callback)));
}

void PinFactorEditor::OnIsPinConfiguredForRemove(
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool is_pin_configured) {
  if (!is_pin_configured) {
    LOG(WARNING)
        << "No PIN configured, ignoring PinFactorEditor::RemovePin call";
    std::move(callback).Run(mojom::ConfigureResult::kSuccess);
    return;
  }

  AccountId account_id = context->GetAccountId();
  pin_backend_->Remove(account_id, auth_token,
                       base::BindOnce(&PinFactorEditor::OnPinConfigured,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(context), std::move(callback)));
}

void PinFactorEditor::OnPinConfigured(
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success) {
  if (!success) {
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {mojom::AuthFactor::kPin}, std::move(context), std::move(callback));
}

void PinFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::PinFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace ash::auth
