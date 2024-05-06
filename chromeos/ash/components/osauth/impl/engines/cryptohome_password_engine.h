// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_PASSWORD_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_PASSWORD_ENGINE_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"

namespace ash {

// This class implements engine for Cryptohome-based Password factor.
class CryptohomePasswordEngine : public CryptohomeBasedEngine {
 public:
  explicit CryptohomePasswordEngine(CryptohomeCore& core);
  ~CryptohomePasswordEngine() override;

  // AuthFactorEngine:
  bool IsDisabledByPolicy() override;
  bool IsLockedOut() override;
  bool IsFactorSpecificRestricted() override;

  void PerformPasswordAttempt(const std::string& raw_password);
  // CryptohomeCore::Client
  void OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) override;
  // CryptohomeBasedEngine:
  std::optional<cryptohome::AuthFactorRef> LookUpFactor(
      UserContext& context) override;

 private:
  void OnAuthAttempt(std::unique_ptr<UserContext>,
                     std::optional<AuthenticationError>);
  void PerformAuthenticationAttempt(const std::string& raw_password,
                                    std::unique_ptr<UserContext> context);

  base::WeakPtrFactory<CryptohomePasswordEngine> weak_factory_{this};
};

class CryptohomePasswordEngineFactory : public AuthFactorEngineFactory {
 public:
  CryptohomePasswordEngineFactory();
  ~CryptohomePasswordEngineFactory() override;
  AshAuthFactor GetFactor() override;
  std::unique_ptr<AuthFactorEngine> CreateEngine(AuthHubMode mode) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_PASSWORD_ENGINE_H_
