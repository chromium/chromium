// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_factor_configuration_helper.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_policy_utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthFactorConfigurationHelper::AuthFactorConfigurationHelper()
    : editor_(std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get())) {}

AuthFactorConfigurationHelper::~AuthFactorConfigurationHelper() = default;

void AuthFactorConfigurationHelper::GetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  editor_->GetAuthFactorsConfiguration(
      std::move(context),
      base::BindOnce(
          [](AuthOperationCallback cb, std::unique_ptr<UserContext> ctx,
             std::optional<AuthenticationError> err) {
            if (err.has_value()) {
              LOG(ERROR) << "GetAuthFactorsConfiguration failed: "
                         << err->get_cryptohome_error();
            } else {
              VLOG(1) << "GetAuthFactorsConfiguration success";
            }
            std::move(cb).Run(std::move(ctx), std::move(err));
          },
          std::move(callback)));
}

void AuthFactorConfigurationHelper::CheckHasAuthFactors(
    const AccountId& account_id,
    CheckHasAuthFactorsCallback callback) {
  VLOG(1) << "Checking auth factors";
  auto user_context = std::make_unique<UserContext>();
  user_context->SetAccountId(account_id);
  GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(
          &AuthFactorConfigurationHelper::OnGetAuthFactorsConfiguration,
          base::Unretained(this), std::move(callback)));
}

void AuthFactorConfigurationHelper::CheckHasOnlinePasswordAndContinue(
    const AccountId& account_id,
    base::OnceClosure on_has_online_password,
    base::OnceClosure on_no_online_password) {
  CheckHasAuthFactors(
      account_id,
      base::BindOnce(
          [](base::OnceClosure has_pw, base::OnceClosure no_pw,
             AuthFactorsSet factors) {
            if (factors.Has(AshAuthFactor::kGaiaPassword)) {
              std::move(has_pw).Run();
            } else {
              std::move(no_pw).Run();
            }
          },
          std::move(on_has_online_password), std::move(on_no_online_password)));
}

void AuthFactorConfigurationHelper::OnGetAuthFactorsConfiguration(
    CheckHasAuthFactorsCallback callback,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to check auth factors  "
               << error->get_cryptohome_error();
    std::move(callback).Run(AuthFactorsSet());
    return;
  }

  const auto& config = context->GetAuthFactorsConfiguration();
  AuthFactorsSet result;

  for (const auto& factor : config.GetConfiguredFactors()) {
    switch (factor.ref().type()) {
      case cryptohome::AuthFactorType::kPassword:
        if (ash::IsGaiaPassword(factor)) {
          result.Put(AshAuthFactor::kGaiaPassword);
        } else if (ash::IsLocalPassword(factor)) {
          result.Put(AshAuthFactor::kLocalPassword);
        } else {
          LOG(ERROR)
              << "Password configured which isn't a local or online password";
        }
        break;
      case cryptohome::AuthFactorType::kPin:
        result.Put(AshAuthFactor::kCryptohomePin);
        break;
      case cryptohome::AuthFactorType::kRecovery:
        result.Put(AshAuthFactor::kRecovery);
        break;
      case cryptohome::AuthFactorType::kSmartCard:
        result.Put(AshAuthFactor::kSmartCard);
        break;
      case cryptohome::AuthFactorType::kLegacyFingerprint:
      case cryptohome::AuthFactorType::kFingerprint:
      case cryptohome::AuthFactorType::kKiosk:
      case cryptohome::AuthFactorType::kUnknownLegacy:
        // Fingerprint, Kiosk and UnknownLegacy factors are not supported by the
        // AuthFactorConfigurationHelper yet.
        NOTIMPLEMENTED();
        break;
    }
  }

  VLOG(1) << "Successfully checked auth factors";
  std::move(callback).Run(result);
}

}  // namespace ash
