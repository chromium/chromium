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
                                 PinBackendDelegate* pin_backend)
    : auth_factor_config_(auth_factor_config),
      pin_backend_(pin_backend),
      auth_factor_editor_(UserDataAuthClient::Get()) {
  CHECK(auth_factor_config_);
  CHECK(pin_backend_);
}

PinFactorEditor::~PinFactorEditor() = default;

void PinFactorEditor::SetPin(
    const std::string& auth_token,
    const std::string& pin,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  ObtainContext(auth_token,
                base::BindOnce(&PinFactorEditor::SetPinWithContext,
                               weak_factory_.GetWeakPtr(), auth_token, pin,
                               std::move(callback)));
}

void PinFactorEditor::RemovePin(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  ObtainContext(auth_token,
                base::BindOnce(&PinFactorEditor::RemovePinWithContext,
                               weak_factory_.GetWeakPtr(), auth_token,
                               std::move(callback)));
}

void PinFactorEditor::ObtainContext(
    const std::string& auth_token,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> callback) {

  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    std::move(callback).Run(nullptr);
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(FROM_HERE, auth_token,
                                              std::move(callback));
}

void PinFactorEditor::SetPinWithContext(
    const std::string& auth_token,
    const std::string& pin,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  AccountId account_id = context->GetAccountId();
  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
  pin_backend_->Set(account_id, auth_token, pin,
                    base::BindOnce(&PinFactorEditor::OnPinConfigured,
                                   weak_factory_.GetWeakPtr(), auth_token,
                                   std::move(callback)));
}

void PinFactorEditor::RemovePinWithContext(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  AccountId account_id = context->GetAccountId();

  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
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
  ObtainContext(auth_token,
                base::BindOnce(&PinFactorEditor::OnPinConfiguredWithContext,
                               weak_factory_.GetWeakPtr(), auth_token,
                               std::move(callback), success));
}

void PinFactorEditor::OnPinConfiguredWithContext(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
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
