// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/osauth/impl/auth_surface_registry.h"
#include "chromeos/ash/components/osauth/impl/legacy_auth_surface_registry.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

class AuthFactorEngineFactory;
class AuthHub;
class AuthSessionStorage;
class AuthFactorPresenceCache;
class CryptohomeCore;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthPartsImpl
    : public AuthParts {
 public:
  // Creates an empty instance to be used in unit tests.
  static std::unique_ptr<AuthPartsImpl> CreateTestInstance();

  AuthPartsImpl();
  ~AuthPartsImpl() override;

  // AuthParts implementation:
  AuthSessionStorage* GetAuthSessionStorage() override;
  AuthHub* GetAuthHub() override;
  CryptohomeCore* GetCryptohomeCore() override;
  LegacyAuthSurfaceRegistry* GetLegacyAuthSurfaceRegistry() override;
  AuthSurfaceRegistry* GetAuthSurfaceRegistry() override;
  void RegisterEngineFactory(
      std::unique_ptr<AuthFactorEngineFactory> factory) override;
  const std::vector<std::unique_ptr<AuthFactorEngineFactory>>&
  GetEngineFactories() override;
  void RegisterEarlyLoginAuthPolicyConnector(
      std::unique_ptr<AuthPolicyConnector> connector) override;
  void ReleaseEarlyLoginAuthPolicyConnector() override;

  void SetProfilePrefsAuthPolicyConnector(
      AuthPolicyConnector* connector) override;
  AuthPolicyConnector* GetAuthPolicyConnector() override;
  void Shutdown() override;

  // Test-related setters:
  void SetAuthHub(std::unique_ptr<AuthHub> auth_hub);
  void SetAuthSessionStorage(std::unique_ptr<AuthSessionStorage> storage);

 private:
  friend class AuthParts;

  void CreateDefaultComponents(PrefService* local_state);

  std::unique_ptr<AuthFactorPresenceCache> factors_cache_;
  std::unique_ptr<CryptohomeCore> cryptohome_core_;
  std::unique_ptr<AuthSessionStorage> session_storage_;
  std::unique_ptr<AuthPolicyConnector> login_screen_policy_connector_;
  std::unique_ptr<AuthPolicyConnector> early_login_policy_connector_;
  raw_ptr<AuthPolicyConnector> profile_prefs_policy_connector_ = nullptr;
  std::unique_ptr<LegacyAuthSurfaceRegistry> legacy_auth_surface_registry_;
  std::unique_ptr<AuthSurfaceRegistry> auth_surface_registry_;

  std::vector<std::unique_ptr<AuthFactorEngineFactory>> engine_factories_;
  std::unique_ptr<AuthHub> auth_hub_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_
