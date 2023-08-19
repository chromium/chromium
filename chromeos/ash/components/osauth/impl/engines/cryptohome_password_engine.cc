// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_password_engine.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

CryptohomePasswordEngine::CryptohomePasswordEngine(CryptohomeCore& core)
    : CryptohomeBasedEngine(core, AshAuthFactor::kGaiaPassword) {}

CryptohomePasswordEngine::~CryptohomePasswordEngine() = default;

absl::optional<cryptohome::AuthFactorRef>
CryptohomePasswordEngine::LookUpFactor(UserContext& context) {
  const cryptohome::AuthFactor* password_factor =
      context.GetAuthFactorsData().FindOnlinePasswordFactor();
  if (!password_factor) {
    return absl::nullopt;
  }
  return password_factor->ref();
}

void CryptohomePasswordEngine::OnAuthFactorUpdate(
    cryptohome::AuthFactorRef factor) {}

bool CryptohomePasswordEngine::IsDisabledByPolicy() {
  return false;
}

bool CryptohomePasswordEngine::IsLockedOut() {
  return false;
}

bool CryptohomePasswordEngine::IsFactorSpecificRestricted() {
  return false;
}

void CryptohomePasswordEngine::PerformPasswordAttempt(
    const std::string& raw_password) {
  if (get_usage_allowed() != UsageAllowed::kEnabled) {
    LOG(ERROR) << "Ignoring password attempt as factor is disabled";
    return;
  }
  CHECK(get_ref().has_value());
  get_observer()->OnFactorAttempt(GetFactor());
  get_core()->GetAuthPerformer()->AuthenticateWithPassword(
      get_ref()->label().value(), raw_password, get_core()->BorrowContext(),
      base::BindOnce(&CryptohomePasswordEngine::OnAuthAttempt,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomePasswordEngine::OnAuthAttempt(
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  get_core()->ReturnContext(std::move(context));
  get_observer()->OnFactorAttemptResult(GetFactor(),
                                        /* success= */ !error.has_value());
}

CryptohomePasswordEngineFactory::CryptohomePasswordEngineFactory() = default;

CryptohomePasswordEngineFactory::~CryptohomePasswordEngineFactory() = default;

AshAuthFactor CryptohomePasswordEngineFactory::GetFactor() {
  return AshAuthFactor::kGaiaPassword;
}

std::unique_ptr<AuthFactorEngine> CryptohomePasswordEngineFactory::CreateEngine(
    AuthHubMode mode) {
  CHECK(CryptohomeCore::Get());
  return std::make_unique<CryptohomePasswordEngine>(*CryptohomeCore::Get());
}

}  // namespace ash
