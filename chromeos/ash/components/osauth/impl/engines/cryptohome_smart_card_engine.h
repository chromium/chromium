// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_SMART_CARD_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_SMART_CARD_ENGINE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"

namespace ash {

// This class implements engine for Cryptohome-based Smart Card AuthFactor.
class CryptohomeSmartCardEngine : public CryptohomeBasedEngine {
 public:
  explicit CryptohomeSmartCardEngine(CryptohomeCore& core);
  ~CryptohomeSmartCardEngine() override;

  // AuthFactorEngine
  bool IsDisabledByPolicy() override;
  bool IsLockedOut() override;
  bool IsFactorSpecificRestricted() override;

  void OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) override;
  // CryptohomeBasedEngine
  std::optional<cryptohome::AuthFactorRef> LookUpFactor(
      UserContext& context) override;

 private:
  void OnAuthAttempt(std::unique_ptr<UserContext>,
                     std::optional<AuthenticationError>);

  base::WeakPtrFactory<CryptohomeSmartCardEngine> weak_factory_{this};
};

class CryptohomeSmartCardEngineFactory : public AuthFactorEngineFactory {
 public:
  explicit CryptohomeSmartCardEngineFactory();
  ~CryptohomeSmartCardEngineFactory() override;
  AshAuthFactor GetFactor() override;
  std::unique_ptr<AuthFactorEngine> CreateEngine(AuthHubMode mode) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_SMART_CARD_ENGINE_H_
