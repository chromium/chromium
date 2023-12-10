// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/password_update_flow.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/operation_chain_runner.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"

namespace ash {

PasswordUpdateFlow::PasswordUpdateFlow()
    : auth_performer_(UserDataAuthClient::Get()),
      auth_factor_editor_(UserDataAuthClient::Get()) {}

PasswordUpdateFlow::~PasswordUpdateFlow() = default;

void PasswordUpdateFlow::Start(std::unique_ptr<UserContext> user_context,
                               const std::string& old_password,
                               AuthSuccessCallback success_callback,
                               AuthErrorCallback error_callback) {
  DCHECK(user_context);
  DCHECK(user_context->GetAuthSessionId().empty());
  LOGIN_LOG(USER) << "Attempting to update user password";

  bool is_ephemeral_user =
      user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          user_context->GetAccountId());

  auth_performer_.StartAuthSession(
      std::move(user_context), is_ephemeral_user, AuthSessionIntent::kDecrypt,
      base::BindOnce(&PasswordUpdateFlow::ContinueWithAuthSession,
                     weak_factory_.GetWeakPtr(), old_password,
                     std::move(success_callback), std::move(error_callback)));
}

void PasswordUpdateFlow::ContinueWithAuthSession(
    const std::string& old_password,
    AuthSuccessCallback success_callback,
    AuthErrorCallback error_callback,
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  DCHECK(user_context);

  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting AuthSession for key migration "
                     << error.value().get_cryptohome_code();
    std::move(error_callback).Run(std::move(user_context), error.value());
    return;
  }
  DCHECK(user_exists);
  DCHECK(!user_context->GetAuthSessionId().empty());

  auto* password_factor =
      user_context->GetAuthFactorsData().FindOnlinePasswordFactor();
  DCHECK(password_factor);
  std::string key_label = password_factor->ref().label().value();

  if (!user_context->HasReplacementKey()) {
    // Make sure that the key has correct label.
    user_context->GetKey()->SetLabel(key_label);
    user_context->SaveKeyForReplacement();
  }

  Key auth_key(old_password);
  auth_key.SetLabel(key_label);
  user_context->SetKey(auth_key);

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&AuthPerformer::AuthenticateUsingKnowledgeKey,
                                 auth_performer_.AsWeakPtr()));
  steps.push_back(base::BindOnce(&AuthFactorEditor::ReplaceContextKey,
                                 auth_factor_editor_.AsWeakPtr()));
  RunOperationChain(std::move(user_context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

}  // namespace ash
