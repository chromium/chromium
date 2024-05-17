// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_BASED_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_BASED_ENGINE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "components/account_id/account_id.h"

namespace ash {

// This class is a common base for Cryptohome-based factors.
class CryptohomeBasedEngine : public AuthFactorEngine,
                              public CryptohomeCore::Client {
 public:
  CryptohomeBasedEngine(CryptohomeCore& core, AshAuthFactor factor);
  ~CryptohomeBasedEngine() override;

  AshAuthFactor GetFactor() const override;

  void InitializeCommon(CommonInitCallback callback) override;
  void ShutdownCommon(ShutdownCallback callback) override;

  void StartAuthFlow(const AccountId& account,
                     AuthPurpose purpose,
                     FactorEngineObserver* observer) override;
  AuthProofToken StoreAuthenticationContext() override;
  void CleanUp(CleanupCallback callback) override;
  void UpdateObserver(FactorEngineObserver* observer) override;
  void StopAuthFlow(ShutdownCallback callback) override;

  void SetUsageAllowed(UsageAllowed usage) override;

  // CryptohomeCore::Client
  void OnCryptohomeAuthSessionStarted() override;
  void OnAuthSessionStartFailure() override;
  void OnCryptohomeAuthSessionFinished() override;

 protected:
  // Subclasses should provide a method that looks up particular
  // cryptohome key reference for the
  virtual std::optional<cryptohome::AuthFactorRef> LookUpFactor(
      UserContext& context) = 0;

  FactorEngineObserver* get_observer() const { return observer_; }
  CryptohomeCore* get_core() const { return core_; }
  std::optional<cryptohome::AuthFactorRef> get_ref() const { return key_ref_; }
  UsageAllowed get_usage_allowed() const { return usage_allowed_; }

 private:
  void OnCryptohomeReady(CommonInitCallback callback, bool service_available);
  void OnSessionStarted();
  void OnAuthAttempt(std::unique_ptr<UserContext>,
                     std::optional<AuthenticationError>);

  raw_ptr<FactorEngineObserver> observer_;
  raw_ptr<CryptohomeCore> core_;
  UsageAllowed usage_allowed_ = UsageAllowed::kDisabled;
  ShutdownCallback shutdown_callback_;

  std::optional<cryptohome::AuthFactorRef> key_ref_;
  AshAuthFactor factor_;
  base::WeakPtrFactory<CryptohomeBasedEngine> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_BASED_ENGINE_H_
