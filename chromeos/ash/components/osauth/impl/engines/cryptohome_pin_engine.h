// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_PIN_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_PIN_ENGINE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

// This class implements engine for Cryptohome-based PIN factor.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) CryptohomePinEngine
    : public CryptohomeBasedEngine {
 public:
  explicit CryptohomePinEngine(CryptohomeCore& core, PrefService* local_state);
  ~CryptohomePinEngine() override;

  // AuthFactorEngine
  bool IsDisabledByPolicy() override;
  bool IsLockedOut() override;
  bool IsFactorSpecificRestricted() override;

  void PerformPinAttempt(const std::string& raw_pin);
  // CryptohomeCore::Client
  void OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) override;
  // CryptohomeBasedEngine
  std::optional<cryptohome::AuthFactorRef> LookUpFactor(
      UserContext& context) override;

 private:
  void OnAuthAttempt(std::unique_ptr<UserContext>,
                     std::optional<AuthenticationError>);
  void PerformAuthenticationAttempt(const std::string& raw_pin,
                                    std::unique_ptr<UserContext> context);

  std::string GetUserSalt(const AccountId& account_id,
                          PrefService* local_state) const;

  raw_ptr<PrefService> local_state_ = nullptr;

  base::WeakPtrFactory<CryptohomePinEngine> weak_factory_{this};
};

class CryptohomePinEngineFactory : public AuthFactorEngineFactory {
 public:
  explicit CryptohomePinEngineFactory(PrefService* local_state);
  ~CryptohomePinEngineFactory() override;
  AshAuthFactor GetFactor() override;
  std::unique_ptr<AuthFactorEngine> CreateEngine(AuthHubMode mode) override;

 private:
  raw_ptr<PrefService> local_state_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_PIN_ENGINE_H_
