// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_pin_engine.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/logging.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

CryptohomePinEngine::CryptohomePinEngine(CryptohomeCore& core,
                                         PrefService* local_state)
    : CryptohomeBasedEngine(core, AshAuthFactor::kCryptohomePin),
      local_state_(local_state) {}

CryptohomePinEngine::~CryptohomePinEngine() = default;

std::optional<cryptohome::AuthFactorRef> CryptohomePinEngine::LookUpFactor(
    UserContext& context) {
  const cryptohome::AuthFactor* pin_factor =
      context.GetAuthFactorsData().FindPinFactor();
  if (!pin_factor) {
    return std::nullopt;
  }
  return pin_factor->ref();
}

void CryptohomePinEngine::OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) {
}

bool CryptohomePinEngine::IsDisabledByPolicy() {
  return false;
}

bool CryptohomePinEngine::IsLockedOut() {
  return false;
}

bool CryptohomePinEngine::IsFactorSpecificRestricted() {
  return false;
}

void CryptohomePinEngine::PerformPinAttempt(const std::string& raw_pin) {
  if (get_usage_allowed() != UsageAllowed::kEnabled) {
    LOG(ERROR) << "Ignoring pin attempt as factor is disabled";
    return;
  }
  CHECK(get_ref().has_value());
  get_observer()->OnFactorAttempt(GetFactor());
  get_core()->BorrowContext(
      base::BindOnce(&CryptohomePinEngine::PerformAuthenticationAttempt,
                     weak_factory_.GetWeakPtr(), raw_pin));
}

void CryptohomePinEngine::OnAuthAttempt(
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  get_core()->ReturnContext(std::move(context));
  get_observer()->OnFactorAttemptResult(GetFactor(),
                                        /* success= */ !error.has_value());
}

void CryptohomePinEngine::PerformAuthenticationAttempt(
    const std::string& raw_pin,
    std::unique_ptr<UserContext> context) {
  const AccountId& account_id = context->GetAccountId();
  get_core()->GetAuthPerformer()->AuthenticateWithPin(
      raw_pin, GetUserSalt(account_id, local_state_), std::move(context),
      base::BindOnce(&CryptohomePinEngine::OnAuthAttempt,
                     weak_factory_.GetWeakPtr()));
}

std::string CryptohomePinEngine::GetUserSalt(const AccountId& account_id,
                                             PrefService* local_state) const {
  user_manager::KnownUser known_user(local_state);
  if (const std::string* salt =
          known_user.FindStringPath(account_id, prefs::kQuickUnlockPinSalt)) {
    return *salt;
  }
  return {};
}

CryptohomePinEngineFactory::CryptohomePinEngineFactory(PrefService* local_state)
    : local_state_(local_state) {}

CryptohomePinEngineFactory::~CryptohomePinEngineFactory() = default;

AshAuthFactor CryptohomePinEngineFactory::GetFactor() {
  return AshAuthFactor::kCryptohomePin;
}

std::unique_ptr<AuthFactorEngine> CryptohomePinEngineFactory::CreateEngine(
    AuthHubMode mode) {
  CHECK(CryptohomeCore::Get());
  return std::make_unique<CryptohomePinEngine>(*CryptohomeCore::Get(),
                                               local_state_);
}

}  // namespace ash
