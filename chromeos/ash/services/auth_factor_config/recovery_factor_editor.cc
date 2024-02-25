// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

RecoveryFactorEditor::RecoveryFactorEditor(AuthFactorConfig* auth_factor_config)
    : auth_factor_config_(auth_factor_config),
      auth_factor_editor_(UserDataAuthClient::Get()) {
  DCHECK(auth_factor_config_);
}
RecoveryFactorEditor::~RecoveryFactorEditor() = default;

void RecoveryFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void RecoveryFactorEditor::Configure(
    const std::string& auth_token,
    bool enabled,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  auth_factor_config_->IsEditable(
      auth_token, mojom::AuthFactor::kRecovery,
      base::BindOnce(&RecoveryFactorEditor::OnGetEditable,
                     weak_factory_.GetWeakPtr(), auth_token, enabled,
                     std::move(callback)));
}

void RecoveryFactorEditor::OnGetEditable(
    const std::string& auth_token,
    bool should_enable,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool is_editable) {
  if (!is_editable) {
    LOG(ERROR) << "Recovery configuration not editable";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(
      FROM_HERE, auth_token,
      base::BindOnce(&RecoveryFactorEditor::ConfigureWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, should_enable,
                     std::move(callback), is_editable));
}

void RecoveryFactorEditor::ConfigureWithContext(
    const std::string& auth_token,
    bool should_enable,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    bool is_editable,
    std::unique_ptr<UserContext> user_context) {
  const bool currently_enabled =
      user_context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kRecovery);

  if (should_enable == currently_enabled) {
    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(user_context));
    std::move(callback).Run(mojom::ConfigureResult::kSuccess);
    return;
  }

  auto on_configured_callback = base::BindOnce(
      &RecoveryFactorEditor::OnRecoveryFactorConfigured,
      weak_factory_.GetWeakPtr(), std::move(callback), auth_token);

  if (should_enable) {
    auth_factor_editor_.AddRecoveryFactor(std::move(user_context),
                                          std::move(on_configured_callback));
  } else {
    auth_factor_editor_.RemoveRecoveryFactor(std::move(user_context),
                                             std::move(on_configured_callback));
  }
}

void RecoveryFactorEditor::OnRecoveryFactorConfigured(
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    if (error->get_cryptohome_code() ==
        user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN) {
      // Handle expired auth session gracefully.
      ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
      ash::AuthSessionStorage::Get()->Invalidate(auth_token, base::DoNothing());
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }

    LOG(ERROR) << "Configuring recovery factor failed, code "
               << error->get_cryptohome_code();
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {mojom::AuthFactor::kRecovery}, auth_token, std::move(context),
      std::move(callback));
}

}  // namespace ash::auth
