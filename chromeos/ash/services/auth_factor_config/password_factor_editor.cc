// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/user_manager/user_manager.h"

namespace ash::auth {

namespace {

const std::size_t kLocalPasswordMinimumLength = 8;

// The synchronous implementation of `CheckLocalPasswordComplexity`. The
// provided `password` string must be valid UTF-8.
mojom::PasswordComplexity CheckLocalPasswordComplexityImpl(
    const std::string& password) {
  // We're counting unicode points here because we already have a function for
  // that, but graphemes might be closer to the user's understanding of what
  // the length of a string is.
  absl::optional<size_t> unicode_size =
      base::CountUnicodeCharacters(password.data(), password.size());
  CHECK(unicode_size.has_value());

  const mojom::PasswordComplexity complexity =
      *unicode_size < kLocalPasswordMinimumLength
          ? mojom::PasswordComplexity::kTooShort
          : mojom::PasswordComplexity::kOk;
  return complexity;
}

}  // namespace

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
  // Mojo strings are valid UTF-8, so the `CheckLocalPasswordComplexityImpl`
  // call is OK.
  if (CheckLocalPasswordComplexityImpl(new_password) !=
      mojom::PasswordComplexity::kOk) {
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  std::unique_ptr<UserContext> user_context;

  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
      LOG(ERROR) << "Invalid auth token";
      std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
      return;
    }
    ash::AuthSessionStorage::Get()->BorrowAsync(
        FROM_HERE, auth_token,
        base::BindOnce(&PasswordFactorEditor::SetLocalPasswordWithContext,
                       weak_factory_.GetWeakPtr(), auth_token, new_password,
                       std::move(callback)));
    return;
  }

  const auto* user = ::user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(user);
  auto* user_context_ptr =
      quick_unlock_storage_->GetUserContext(user, auth_token);
  if (user_context_ptr == nullptr) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  SetLocalPasswordWithContext(auth_token, new_password, std::move(callback),
                              std::make_unique<UserContext>(*user_context_ptr));
}

void PasswordFactorEditor::SetLocalPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> user_context) {
  if (!user_context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  const cryptohome::AuthFactor* password_factor =
      user_context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  if (!password_factor) {
    // The user doesn't have a password yet (neither Gaia nor local).
    // TODO(b/290916811): Add a new local password factor here and return
    // success.
    LOG(ERROR) << "No existing password, will not add local password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(user_context),
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
        auth_token, std::move(user_context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_editor_.ReplaceLocalPasswordFactor(
      std::move(user_context), cryptohome::RawPassword(new_password),
      base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     auth_token));
}

void PasswordFactorEditor::CheckLocalPasswordComplexity(
    const std::string& password,
    base::OnceCallback<void(mojom::PasswordComplexity)> callback) {
  // Mojo strings are valid UTF-8, so the `CheckLocalPasswordComplexityImpl`
  // call is OK.
  std::move(callback).Run(CheckLocalPasswordComplexityImpl(password));
}

void PasswordFactorEditor::BindReceiver(
    mojo::PendingReceiver<mojom::PasswordFactorEditor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PasswordFactorEditor::OnPasswordConfigured(
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error) {
    LOG(ERROR) << "Failed to configure password, code "
               << error->get_cryptohome_code();
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {mojom::AuthFactor::kGaiaPassword, mojom::AuthFactor::kLocalPassword},
      auth_token, std::move(context), std::move(callback));
}

}  // namespace ash::auth
