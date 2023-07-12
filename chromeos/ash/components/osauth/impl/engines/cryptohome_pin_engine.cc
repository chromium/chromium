// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_pin_engine.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

CryptohomePinEngine::CryptohomePinEngine(CryptohomeCore& core,
                                         PrefService* local_state)
    : CryptohomeBasedEngine(core, AshAuthFactor::kCryptohomePin),
      local_state_(local_state) {}

CryptohomePinEngine::~CryptohomePinEngine() = default;

absl::optional<cryptohome::AuthFactorRef> CryptohomePinEngine::LookUpFactor(
    UserContext& context) {
  const cryptohome::AuthFactor* pin_factor =
      context.GetAuthFactorsData().FindPinFactor();
  if (!pin_factor) {
    return absl::nullopt;
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
  const AccountId& account_id = get_core()->GetCurrentContext()->GetAccountId();
  get_observer()->OnFactorAttempt(GetFactor());
  get_core()->GetAuthPerformer()->AuthenticateWithPin(
      raw_pin, GetUserSalt(account_id, local_state_),
      get_core()->BorrowContext(),
      base::BindOnce(&CryptohomePinEngine::OnAuthAttempt,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomePinEngine::OnAuthAttempt(
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  get_core()->ReturnContext(std::move(context));
  get_observer()->OnFactorAttemptResult(GetFactor(),
                                        /* success= */ !error.has_value());
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
