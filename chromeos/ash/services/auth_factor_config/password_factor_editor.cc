// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"

#include <string>
#include "base/functional/callback.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

PasswordFactorEditor::PasswordFactorEditor(AuthFactorConfig* auth_factor_config,
                                           QuickUnlockStorageDelegate* storage)
    : auth_factor_config_(auth_factor_config),
      quick_unlock_storage_(storage),
      auth_factor_editor_(UserDataAuthClient::Get()) {
  CHECK(auth_factor_config_);
  CHECK(quick_unlock_storage_);
}

PasswordFactorEditor::~PasswordFactorEditor() = default;

void PasswordFactorEditor::SetLocalPassword(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  auto* user_context_ptr =
      quick_unlock_storage_->GetUserContext(user, auth_token);
  if (user_context_ptr == nullptr) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  auto context = std::make_unique<UserContext>(*user_context_ptr);

  const cryptohome::AuthFactor* password_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  if (!password_factor) {
    // The user doesn't have a password yet (neither Gaia nor local).
    // TODO(b/290916811): Add a new local password factor here and return
    // success.
    LOG(ERROR) << "No existing password, will not add local password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  if (!IsLocalPassword(*password_factor)) {
    // TODO(b/290916811):  *Atomically* replace the Gaia password factor with
    // a local password factor.
    LOG(ERROR) << "Current password is not local, will not replace with local "
                  "password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_editor_.ReplaceLocalPasswordFactor(
      std::move(context), cryptohome::RawPassword(new_password),
      base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PasswordFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::PasswordFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PasswordFactorEditor::OnPasswordConfigured(
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error) {
    LOG(ERROR) << "Failed to configure password, code "
               << error->get_cryptohome_code();
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {mojom::AuthFactor::kGaiaPassword, mojom::AuthFactor::kLocalPassword},
      std::move(context), std::move(callback));
}

}  // namespace ash::auth
