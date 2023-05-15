// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

RecoveryFactorEditor::RecoveryFactorEditor(
    AuthFactorConfig* auth_factor_config,
    QuickUnlockStorageDelegate* quick_unlock_storage)
    : auth_factor_config_(auth_factor_config),
      quick_unlock_storage_(quick_unlock_storage) {
  DCHECK(auth_factor_config_);
  DCHECK(quick_unlock_storage_);
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
  CHECK(features::IsCryptohomeRecoveryEnabled());

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

  const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
  auto* user_context_ptr =
      quick_unlock_storage_->GetUserContext(user, auth_token);
  if (user_context_ptr == nullptr) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  const bool currently_enabled =
      user_context_ptr->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kRecovery);

  if (should_enable == currently_enabled) {
    std::move(callback).Run(mojom::ConfigureResult::kSuccess);
    return;
  }

  auto user_context = std::make_unique<UserContext>(*user_context_ptr);

  auto on_configured_callback =
      base::BindOnce(&RecoveryFactorEditor::OnRecoveryFactorConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback));

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
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    // Handle expired auth session gracefully.
    if (error->get_cryptohome_code() ==
        user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN) {
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }

    LOG(ERROR) << "Configuring recovery factor failed, code "
               << error->get_cryptohome_code();
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(&RecoveryFactorEditor::OnGetAuthFactorsConfiguration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RecoveryFactorEditor::OnGetAuthFactorsConfiguration(
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Refreshing list of configured auth factors failed, code "
               << error->get_cryptohome_code();
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
  quick_unlock_storage_->SetUserContext(user, std::move(context));

  std::move(callback).Run(mojom::ConfigureResult::kSuccess);
  auth_factor_config_->NotifyFactorObservers(mojom::AuthFactor::kRecovery);
}

}  // namespace ash::auth
