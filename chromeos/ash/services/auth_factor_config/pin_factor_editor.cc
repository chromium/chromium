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
      quick_unlock_storage_(storage) {
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

  pin_backend_->Set(
      user->GetAccountId(), auth_token, pin,
      base::BindOnce(&PinFactorEditor::OnPinConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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

  pin_backend_->Remove(
      user->GetAccountId(), auth_token,
      base::BindOnce(&PinFactorEditor::OnPinConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PinFactorEditor::OnPinConfigured(
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success) {
  const mojom::ConfigureResult result =
      success ? mojom::ConfigureResult::kSuccess
              : mojom::ConfigureResult::kFatalError;
  std::move(callback).Run(result);
  auth_factor_config_->NotifyFactorObservers(mojom::AuthFactor::kPin);
}

void PinFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::PinFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace ash::auth
