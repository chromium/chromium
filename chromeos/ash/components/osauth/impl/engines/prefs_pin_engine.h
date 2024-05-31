// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_PREFS_PIN_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_PREFS_PIN_ENGINE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) PrefsPinEngine
    : public AuthFactorEngine,
      public CryptohomeCore::Client {
 public:
  static constexpr int kMaximumUnlockAttempts = 5;

  PrefsPinEngine(CryptohomeCore& core, PrefService& pref_service);

  ~PrefsPinEngine() override;

  // Attempt to perform an PIN authentication.
  void PerformPinAttempt(const std::string& raw_pin);

 private:
  // Functions to implement AuthFactorEngine.
  AshAuthFactor GetFactor() const override;
  void InitializeCommon(CommonInitCallback callback) override;
  void ShutdownCommon(ShutdownCallback callback) override;
  void StartAuthFlow(const AccountId& account,
                     AuthPurpose purpose,
                     FactorEngineObserver* observer) override;
  void UpdateObserver(FactorEngineObserver* observer) override;
  void CleanUp(CleanupCallback callback) override;
  void StopAuthFlow(ShutdownCallback callback) override;
  AuthProofToken StoreAuthenticationContext() override;
  void SetUsageAllowed(UsageAllowed usage) override;
  bool IsDisabledByPolicy() override;
  bool IsLockedOut() override;
  bool IsFactorSpecificRestricted() override;
  void OnSuccessfulAuthentiation() override;

  // Functions to implement CryptohomeCore::Client.
  void OnCryptohomeAuthSessionStarted() override;
  void OnAuthSessionStartFailure() override;
  void OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) override;
  void OnCryptohomeAuthSessionFinished() override;

  // Handles cryptohome readiness during init.
  void OnCryptohomeReady(CommonInitCallback callback, bool service_available);

  raw_ptr<CryptohomeCore> core_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<FactorEngineObserver> observer_;
  UsageAllowed usage_allowed_ = UsageAllowed::kDisabled;
  bool is_supported_ = false;

  ShutdownCallback shutdown_callback_;

  base::WeakPtrFactory<PrefsPinEngine> weak_factory_{this};
};

class PrefsPinEngineFactory : public AuthFactorEngineFactory {
 public:
  explicit PrefsPinEngineFactory(PrefService& local_state);

  AshAuthFactor GetFactor() override;
  std::unique_ptr<AuthFactorEngine> CreateEngine(AuthHubMode mode) override;

 private:
  raw_ptr<PrefService> local_state_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_PREFS_PIN_ENGINE_H_
