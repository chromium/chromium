// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/pin_factor_editor.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
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

void PinFactorEditor::UpdatePin(
    const std::string& auth_token,
    const std::string& pin,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  ObtainContext(auth_token,
                base::BindOnce(&PinFactorEditor::UpdatePinWithContext,
                               weak_factory_.GetWeakPtr(), auth_token, pin,
                               std::move(callback)));
}

void PinFactorEditor::RemovePin(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  auth_factor_config_->CheckConfiguredFactors(
      auth_token,
      {mojom::AuthFactor::kPrefBasedPin, mojom::AuthFactor::kCryptohomePin,
       mojom::AuthFactor::kCryptohomePinV2},
      base::BindOnce(&PinFactorEditor::OnRemovePinConfigured,
                     weak_factory_.GetWeakPtr(), auth_token,
                     std::move(callback)));
}

void PinFactorEditor::GetConfiguredPinFactor(
    const std::string& auth_token,
    base::OnceCallback<void(std::optional<mojom::AuthFactor>)> callback) {
  auth_factor_config_->CheckConfiguredFactors(
      auth_token,
      {mojom::AuthFactor::kPrefBasedPin, mojom::AuthFactor::kCryptohomePin,
       mojom::AuthFactor::kCryptohomePinV2},
      base::BindOnce(&PinFactorEditor::GetConfiguredPinFactorResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PinFactorEditor::GetConfiguredPinFactorResponse(
    base::OnceCallback<void(std::optional<mojom::AuthFactor>)> callback,
    AuthFactorSet factors) {
  CHECK(factors.size() < 2);
  if (factors.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(*factors.begin());
}

void PinFactorEditor::OnRemovePinConfigured(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    AuthFactorSet factors) {
  CHECK(factors.size() < 2);
  if (factors.empty()) {
    LOG(WARNING)
        << "No PIN configured, ignoring PinFactorEditor::RemovePin call";
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }
  ObtainContext(
      auth_token,
      base::BindOnce(&PinFactorEditor::OnRemovePinConfiguredWithContext,
                     weak_factory_.GetWeakPtr(), auth_token,
                     std::move(callback), *factors.begin()));
}

void PinFactorEditor::OnRemovePinConfiguredWithContext(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    mojom::AuthFactor factor,
    std::unique_ptr<UserContext> context) {
  AccountId account_id = context->GetAccountId();
  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

  pin_backend_->Remove(
      account_id, auth_token,
      base::BindOnce(&PinFactorEditor::OnPinRemove, weak_factory_.GetWeakPtr(),
                     auth_token, factor, std::move(callback)));
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

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(ERROR) << "Invalid auth token: user does not exist";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
  pin_backend_->Set(
      account_id, auth_token, pin,
      base::BindOnce(&PinFactorEditor::OnPinSet, weak_factory_.GetWeakPtr(),
                     auth_token, std::move(callback)));
}

void PinFactorEditor::OnPinSet(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success) {
  ObtainContext(auth_token,
                base::BindOnce(&PinFactorEditor::OnPinSetWithContext,
                               weak_factory_.GetWeakPtr(), auth_token,
                               std::move(callback), success));
}

void PinFactorEditor::OnPinSetWithContext(
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
    LOG(ERROR) << "Pin setup failed";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }
  CHECK(context->HasAuthFactorsConfiguration());

  const cryptohome::AuthFactor* pin_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPin);
  bool hasCryptohomePin =
      pin_factor && pin_factor->GetCommonMetadata().lockout_policy() !=
                        cryptohome::LockoutPolicy::kTimeLimited;
  bool hasCryptohomePinV2 =
      pin_factor && pin_factor->GetCommonMetadata().lockout_policy() ==
                        cryptohome::LockoutPolicy::kTimeLimited;

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {hasCryptohomePinV2
           ? mojom::AuthFactor::kCryptohomePinV2
           : (hasCryptohomePin ? mojom::AuthFactor::kCryptohomePin
                               : mojom::AuthFactor::kPrefBasedPin)},
      auth_token, std::move(context), std::move(callback));
}

void PinFactorEditor::UpdatePinWithContext(
    const std::string& auth_token,
    const std::string& pin,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  CHECK(context->HasAuthFactorsConfiguration());

  const cryptohome::AuthFactor* pin_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPin);

  if (!pin_factor) {
    LOG(ERROR) << "Trying to update cryptohome pin while none is present";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  AccountId account_id = context->GetAccountId();

  mojom::AuthFactor old_pin_factor_type =
      pin_factor->GetCommonMetadata().lockout_policy() !=
              cryptohome::LockoutPolicy::kTimeLimited
          ? mojom::AuthFactor::kCryptohomePin
          : mojom::AuthFactor::kCryptohomePinV2;

  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

  pin_backend_->UpdateCryptohomePin(
      account_id, auth_token, pin,
      base::BindOnce(&PinFactorEditor::OnUpdatePinConfigured,
                     weak_factory_.GetWeakPtr(), auth_token,
                     old_pin_factor_type, std::move(callback)));
}

void PinFactorEditor::OnUpdatePinConfigured(
    const std::string& auth_token,
    mojom::AuthFactor old_pin_factor_type,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success) {
  ObtainContext(
      auth_token,
      base::BindOnce(&PinFactorEditor::OnUpdatePinConfiguredWithContext,
                     weak_factory_.GetWeakPtr(), auth_token,
                     old_pin_factor_type, std::move(callback), success));
}

void PinFactorEditor::OnUpdatePinConfiguredWithContext(
    const std::string& auth_token,
    mojom::AuthFactor old_pin_factor_type,
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

  CHECK(context->HasAuthFactorsConfiguration());

  const cryptohome::AuthFactor* pin_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPin);

  CHECK(pin_factor);

  mojom::AuthFactor new_pin_factor_type =
      pin_factor->GetCommonMetadata().lockout_policy() !=
              cryptohome::LockoutPolicy::kTimeLimited
          ? mojom::AuthFactor::kCryptohomePin
          : mojom::AuthFactor::kCryptohomePinV2;

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {old_pin_factor_type, new_pin_factor_type}, auth_token,
      std::move(context), std::move(callback));
}

void PinFactorEditor::OnPinRemove(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success) {
  ObtainContext(auth_token,
                base::BindOnce(&PinFactorEditor::OnPinRemoveWithContext,
                               weak_factory_.GetWeakPtr(), auth_token, factor,
                               std::move(callback), success));
}

void PinFactorEditor::OnPinRemoveWithContext(
    const std::string& auth_token,
    mojom::AuthFactor factor,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool success,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  if (!success) {
    LOG(ERROR) << "Pin remove failed";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }
  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {factor}, auth_token, std::move(context), std::move(callback));
  return;
}

void PinFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::PinFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace ash::auth
