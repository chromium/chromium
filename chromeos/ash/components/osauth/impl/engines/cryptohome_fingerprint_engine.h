// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_FINGERPRINT_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_FINGERPRINT_ENGINE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

// This class implements engine for Cryptohome-based Fingerprint factor.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    CryptohomeFingerprintEngine
    : public CryptohomeBasedEngine,
      public UserDataAuthClient::PrepareAuthFactorProgressObserver {
 public:
  explicit CryptohomeFingerprintEngine(CryptohomeCore& core,
                                       PrefService* local_state);
  ~CryptohomeFingerprintEngine() override;

  // AuthFactorEngine
  bool IsDisabledByPolicy() override;
  bool IsLockedOut() override;
  bool IsFactorSpecificRestricted() override;
  void SetUsageAllowed(UsageAllowed usage) override;
  void CleanUp(CleanupCallback callback) override;

  // CryptohomeCore::Client
  void OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) override;

  // CryptohomeBasedEngine
  // Returns the reference to an arbitrary fingerprint auth factor.
  std::optional<cryptohome::AuthFactorRef> LookUpFactor(
      UserContext& context) override;

  // UserDataAuthClient::PrepareAuthFactorProgressObserver
  void OnFingerprintAuthScan(
      const ::user_data_auth::AuthScanDone& result) override;

  void PerformFingerprintAttempt();

 private:
  enum class Stage {
    kIdle,
    kPreparing,
    kPrepared,
    kTerminating,
  };
  void PerformPreparation(std::unique_ptr<UserContext>);
  void PerformAuthentication(std::unique_ptr<UserContext>);
  void PerformTermination(CleanupCallback callback,
                          std::unique_ptr<UserContext>);
  void OnPrepareAuthFactor(std::unique_ptr<UserContext>,
                           std::optional<AuthenticationError>);
  void OnAuthAttempt(std::unique_ptr<UserContext>,
                     std::optional<AuthenticationError>);
  void OnTerminateAuthFactor(CleanupCallback callback,
                             std::unique_ptr<UserContext>,
                             std::optional<AuthenticationError>);

  raw_ptr<PrefService> local_state_ = nullptr;
  bool buffered_attempt_ = false;
  Stage stage_ = Stage::kIdle;

  base::WeakPtrFactory<CryptohomeFingerprintEngine> weak_factory_{this};
};

class CryptohomeFingerprintEngineFactory : public AuthFactorEngineFactory {
 public:
  explicit CryptohomeFingerprintEngineFactory(PrefService* local_state);
  ~CryptohomeFingerprintEngineFactory() override;
  AshAuthFactor GetFactor() override;
  std::unique_ptr<AuthFactorEngine> CreateEngine(AuthHubMode mode) override;

 private:
  raw_ptr<PrefService> local_state_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_ENGINES_CRYPTOHOME_FINGERPRINT_ENGINE_H_
