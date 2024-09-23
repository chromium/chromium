// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
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
  std::optional<size_t> unicode_size =
      base::CountUnicodeCharacters(password.data(), password.size());
  CHECK(unicode_size.has_value());

  const mojom::PasswordComplexity complexity =
      *unicode_size < kLocalPasswordMinimumLength
          ? mojom::PasswordComplexity::kTooShort
          : mojom::PasswordComplexity::kOk;
  return complexity;
}

}  // namespace

PasswordFactorEditor::PasswordFactorEditor(AuthFactorConfig* auth_factor_config)
    : auth_factor_config_(auth_factor_config),
      auth_factor_editor_(UserDataAuthClient::Get()) {
  CHECK(auth_factor_config_);
}

PasswordFactorEditor::~PasswordFactorEditor() = default;

void PasswordFactorEditor::UpdateOrSetLocalPassword(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  ash::AuthSessionStorage::Get()->BorrowAsync(
      FROM_HERE, auth_token,
      base::BindOnce(&PasswordFactorEditor::UpdateOrSetPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password,
                     cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel},
                     std::move(callback)));
}

void PasswordFactorEditor::UpdateOrSetPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    const cryptohome::KeyLabel& label,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  if (!context) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  // Mojo strings are valid UTF-8, so the `CheckLocalPasswordComplexityImpl`
  // call is OK.
  if (CheckLocalPasswordComplexityImpl(new_password) !=
      mojom::PasswordComplexity::kOk) {
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  CHECK(context->HasAuthFactorsConfiguration());
  if (context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPassword)) {
    // Update.
    UpdatePasswordWithContext(auth_token, new_password, label,
                              std::move(callback), std::move(context));
  } else {
    // Set.
    SetPasswordWithContext(auth_token, new_password, label, std::move(callback),
                           std::move(context));
  }
}

void PasswordFactorEditor::UpdateOrSetOnlinePassword(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }

  ash::AuthSessionStorage::Get()->BorrowAsync(
      FROM_HERE, auth_token,
      base::BindOnce(&PasswordFactorEditor::UpdateOrSetPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password,
                     cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel},
                     std::move(callback)));
}

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

  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(
      FROM_HERE, auth_token,
      base::BindOnce(&PasswordFactorEditor::SetPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password,
                     cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel},
                     std::move(callback)));
}

void PasswordFactorEditor::SetOnlinePassword(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    LOG(ERROR) << "Invalid auth token";
    std::move(callback).Run(mojom::ConfigureResult::kInvalidTokenError);
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(
      FROM_HERE, auth_token,
      base::BindOnce(&PasswordFactorEditor::SetPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password,
                     cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel},
                     std::move(callback)));
}

void PasswordFactorEditor::UpdatePasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    const cryptohome::KeyLabel& label,
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
    LOG(ERROR) << "No existing password, will not add local password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(user_context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  bool is_new_password_local =
      label.value() == kCryptohomeLocalPasswordKeyLabel;
  bool is_old_password_local = IsLocalPassword(*password_factor);
  bool is_label_update_required =
      is_new_password_local != is_old_password_local;

  if (is_label_update_required) {
    if (!features::IsChangePasswordFactorSetupEnabled()) {
      LOG(ERROR)
          << "Switching between online and local password is not supported";
      auth_factor_config_->NotifyFactorObserversAfterFailure(
          auth_token, std::move(user_context),
          base::BindOnce(std::move(callback),
                         mojom::ConfigureResult::kFatalError));
      return;
    }
    if (!is_new_password_local) {
      LOG(ERROR) << "Switching from local to online password is not supported";
      auth_factor_config_->NotifyFactorObserversAfterFailure(
          auth_token, std::move(user_context),
          base::BindOnce(std::move(callback),
                         mojom::ConfigureResult::kFatalError));
      return;
    }
    // Atomically replace the Gaia password factor with a local password
    // factor.
    auth_factor_editor_.ReplacePasswordFactor(
        std::move(user_context), /*old_label=*/password_factor->ref().label(),
        cryptohome::RawPassword(new_password),
        /*new_label=*/cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel},
        base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       auth_token));
  } else {
    // Note that old online factors might have label "legacy-0" instead of
    // "gaia", so we use password_factor->ref().label() here.
    auth_factor_editor_.UpdatePasswordFactor(
        std::move(user_context), cryptohome::RawPassword(new_password),
        password_factor->ref().label(),
        base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       auth_token));
  }
}

void PasswordFactorEditor::SetPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    const cryptohome::KeyLabel& label,
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
  if (password_factor) {
    // The user already has a password factor.
    LOG(ERROR)
        << "Local password factor already exists, will not add local password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(user_context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_editor_.SetPasswordFactor(
      std::move(user_context), cryptohome::RawPassword(new_password),
      std::move(label),
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
    std::optional<AuthenticationError> error) {
  if (error) {
    LOG(ERROR) << "Failed to configure password, code "
               << error->get_cryptohome_code();
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_config_->OnUserHasKnowledgeFactor(*context);

  auth_factor_config_->NotifyFactorObserversAfterSuccess(
      {mojom::AuthFactor::kGaiaPassword, mojom::AuthFactor::kLocalPassword},
      auth_token, std::move(context), std::move(callback));
}

}  // namespace ash::auth
