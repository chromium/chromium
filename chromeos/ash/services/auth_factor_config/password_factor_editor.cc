// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

namespace ash::auth {

namespace {

const std::size_t kLocalPasswordMinimumLength = 8;

using ConfigureResultCallback =
    base::OnceCallback<void(mojom::ConfigureResult)>;
using CheckLocalPasswordComplexityCallback =
    PasswordFactorEditor::CheckLocalPasswordComplexityCallback;

void FailWithInvalidTokenError(base::Location from_here,
                               ConfigureResultCallback result_callback) {
  LOG(ERROR) << "Invalid auth token: " << from_here.ToString();

  std::move(result_callback).Run(mojom::ConfigureResult::kInvalidTokenError);
}

void FailWithInvalidTokenError(
    base::Location from_here,
    CheckLocalPasswordComplexityCallback result_callback) {
  LOG(ERROR) << "Invalid auth token: " << from_here.ToString();

  std::move(result_callback)
      .Run(base::unexpected(mojom::ConfigureResult::kInvalidTokenError));
}

template <typename ResultCallback, typename Continuation>
void OnContextBorrowed(base::Location from_here,
                       ResultCallback result_callback,
                       Continuation continuation_callback,
                       std::unique_ptr<UserContext> context) {
  if (!context) {
    FailWithInvalidTokenError(from_here, std::move(result_callback));
    return;
  }
  std::move(continuation_callback)
      .Run(std::move(result_callback), std::move(context));
}

template <typename ResultCallback, typename Continuation>
void ObtainContextOrFailImpl(base::Location from_here,
                             const std::string& auth_token,
                             ResultCallback result_callback,
                             Continuation continuation_callback) {
  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    FailWithInvalidTokenError(from_here, std::move(result_callback));
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(
      from_here, auth_token,
      base::BindOnce(&OnContextBorrowed<ResultCallback, Continuation>,
                     from_here, std::move(result_callback),
                     std::move(continuation_callback)));
}

#define ObtainContextOrFail(...) ObtainContextOrFailImpl(FROM_HERE, __VA_ARGS__)

// The synchronous implementation of `CheckLocalPasswordComplexity`. The
// provided `password` string must be valid UTF-8.
// Note: Mojo strings are valid UTF-8.
mojom::PasswordComplexity CheckLocalPasswordComplexityImpl(
    const AccountId& account_id,
    const std::string& password) {
  std::optional<LocalAuthFactorsComplexity> policy;
  if (account_id != EmptyAccountId()) {
    policy = AuthParts::Get()
                 ->GetAuthPolicyConnector()
                 ->GetLocalAuthFactorsComplexity(account_id);
  }

  if (policy.has_value()) {
    // LocalAuthFactorsComplexity policy is set, perform the new check.
    bool ok = policy::local_auth_factors::CheckPasswordComplexity(
        password, policy.value());
    return ok ? mojom::PasswordComplexity::kOk
              : mojom::PasswordComplexity::kTooShort;
  }

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

void CheckLocalPasswordComplexityWithContext(
    const std::string& auth_token,
    const std::string& password,
    PasswordFactorEditor::CheckLocalPasswordComplexityCallback callback,
    std::unique_ptr<UserContext> context) {
  AccountId account_id = context->GetAccountId();
  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));

  std::move(callback).Run(
      CheckLocalPasswordComplexityImpl(account_id, password));
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
  ObtainContextOrFail(
      auth_token, std::move(callback),
      base::BindOnce(&PasswordFactorEditor::UpdateOrSetLocalPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password));
}

void PasswordFactorEditor::UpdateOrSetLocalPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  // Check complexity for local password (no complexity check for online
  // passwords as it is checked on the server side by the identity provider).
  if (CheckLocalPasswordComplexityImpl(context->GetAccountId(), new_password) !=
      mojom::PasswordComplexity::kOk) {
    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
    return std::move(callback).Run(mojom::ConfigureResult::kFatalError);
  }

  UpdateOrSetPasswordWithContext(
      auth_token, new_password,
      cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel},
      std::move(callback), std::move(context));
}

void PasswordFactorEditor::UpdateOrSetPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    const cryptohome::KeyLabel& label,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
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
  ObtainContextOrFail(
      auth_token, std::move(callback),
      base::BindOnce(&PasswordFactorEditor::UpdateOrSetPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password,
                     cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel}));
}

void PasswordFactorEditor::SetLocalPassword(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  ObtainContextOrFail(
      auth_token, std::move(callback),
      base::BindOnce(&PasswordFactorEditor::SetLocalPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password));
}

void PasswordFactorEditor::SetOnlinePassword(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  ObtainContextOrFail(
      auth_token, std::move(callback),
      base::BindOnce(&PasswordFactorEditor::SetPasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token, new_password,
                     cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel}));
}

void PasswordFactorEditor::UpdatePasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    const cryptohome::KeyLabel& label,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  const cryptohome::AuthFactor* password_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  if (!password_factor) {
    // The user doesn't have a password yet (neither Gaia nor local).
    LOG(ERROR) << "No existing password, will not update password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
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
    bool policy_does_not_force_online_password =
        !features::IsManagedLocalPinAndPasswordEnabled() ||
        AuthParts::Get()
                ->GetAuthPolicyConnector()
                ->AllowedLocalAuthFactors(context->GetAccountId())
                ->size() > 0;
    // Only allow switching from local password to online password if the policy
    // doesn't allow local auth factors anymore. Note: For unmanaged user there
    // will always be allowed local auth factors.
    if (!is_new_password_local && policy_does_not_force_online_password) {
      LOG(ERROR) << "Switching from local to online password is not supported";
      auth_factor_config_->NotifyFactorObserversAfterFailure(
          auth_token, std::move(context),
          base::BindOnce(std::move(callback),
                         mojom::ConfigureResult::kFatalError));
      return;
    }
    // Atomically replace the Gaia password factor with a local password factor
    // or a local password with a Gaia password.
    const cryptohome::KeyLabel new_label{is_new_password_local
                                             ? kCryptohomeLocalPasswordKeyLabel
                                             : kCryptohomeGaiaKeyLabel};
    auth_factor_editor_.ReplacePasswordFactor(
        std::move(context),
        /*old_label=*/password_factor->ref().label(),
        cryptohome::RawPassword(new_password), new_label,
        base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       auth_token));
  } else {
    // Note that old online factors might have label "legacy-0" instead of
    // "gaia", so we use password_factor->ref().label() here.
    auth_factor_editor_.UpdatePasswordFactor(
        std::move(context), cryptohome::RawPassword(new_password),
        password_factor->ref().label(),
        base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       auth_token));
  }
}

void PasswordFactorEditor::SetLocalPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  // Check complexity for local password (no complexity check for online
  // passwords as it is checked on the server side by the identity provider).
  if (CheckLocalPasswordComplexityImpl(context->GetAccountId(), new_password) !=
      mojom::PasswordComplexity::kOk) {
    ash::AuthSessionStorage::Get()->Return(auth_token, std::move(context));
    return std::move(callback).Run(mojom::ConfigureResult::kFatalError);
  }

  SetPasswordWithContext(auth_token, new_password,
                         cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel},
                         std::move(callback), std::move(context));
}

void PasswordFactorEditor::SetPasswordWithContext(
    const std::string& auth_token,
    const std::string& new_password,
    const cryptohome::KeyLabel& label,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  const cryptohome::AuthFactor* password_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  if (password_factor) {
    // The user already has a password factor.
    LOG(ERROR)
        << "Password factor already exists, will not add online password";
    auth_factor_config_->NotifyFactorObserversAfterFailure(
        auth_token, std::move(context),
        base::BindOnce(std::move(callback),
                       mojom::ConfigureResult::kFatalError));
    return;
  }

  auth_factor_editor_.SetPasswordFactor(
      std::move(context), cryptohome::RawPassword(new_password),
      std::move(label),
      base::BindOnce(&PasswordFactorEditor::OnPasswordConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     auth_token));
}

void PasswordFactorEditor::CheckLocalPasswordComplexity(
    const std::string& auth_token,
    const std::string& password,
    CheckLocalPasswordComplexityCallback callback) {
  if (ash::features::IsLocalFactorsPasswordComplexityEnabled()) {
    ObtainContextOrFail(auth_token, std::move(callback),
                        base::BindOnce(&CheckLocalPasswordComplexityWithContext,
                                       auth_token, password));
    return;
  }

  std::move(callback).Run(
      CheckLocalPasswordComplexityImpl(EmptyAccountId(), password));
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

void PasswordFactorEditor::RemovePassword(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback) {
  ObtainContextOrFail(
      auth_token, std::move(callback),
      base::BindOnce(&PasswordFactorEditor::RemovePasswordWithContext,
                     weak_factory_.GetWeakPtr(), auth_token));
}

void PasswordFactorEditor::RemovePasswordWithContext(
    const std::string& auth_token,
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    std::unique_ptr<UserContext> context) {
  const cryptohome::AuthFactor* password_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  if (!password_factor) {
    // The user doesn't have a password yet (neither Gaia nor local).
    LOG(ERROR) << "No existing password, will not remove password.";
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  const cryptohome::AuthFactor* pin_factor =
      context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPin);

  if (!pin_factor) {
    // The user doesn't have a password to remove.
    LOG(ERROR) << "No existing pin, will not remove password.";
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  } else if (pin_factor->GetCommonMetadata().lockout_policy() !=
             cryptohome::LockoutPolicy::kTimeLimited) {
    LOG(ERROR) << "Cannot remove password, pin is not modern pin";
    std::move(callback).Run(mojom::ConfigureResult::kFatalError);
    return;
  }

  auth_factor_editor_.RemovePasswordFactor(
      std::move(context), password_factor->ref().label(),
      base::BindOnce(&PasswordFactorEditor::OnPasswordRemoved,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     auth_token));
}

void PasswordFactorEditor::OnPasswordRemoved(
    base::OnceCallback<void(mojom::ConfigureResult)> callback,
    const std::string& auth_token,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error) {
    LOG(ERROR) << "Failed to remove password, code "
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
