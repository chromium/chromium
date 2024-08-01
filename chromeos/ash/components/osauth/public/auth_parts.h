// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_PARTS_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_PARTS_H_

#include <memory>
#include <vector>

#include "base/component_export.h"

class PrefService;

namespace ash {

class AuthHub;
class AuthPolicyConnector;
class AuthSessionStorage;
class AuthSurfaceRegistry;
class LegacyAuthSurfaceRegistry;
class AuthFactorEngineFactory;
class CryptohomeCore;

// Central repository for accessing various OS authentication-related
// objects.
// When run normally or as a part of browser_tests it is created and
// owned by the `ChromeBrowserMainPartsAsh`.
// Unit tests can create an empty implementation using
// `AuthPartsImpl::CreateTestInstance`.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthParts {
 public:
  // Creates a global instance. Must be called before any calls to Get().
  static std::unique_ptr<AuthParts> Create(PrefService* local_state);

  // Gets the global instance. Object should be created before that.
  // Value obtained from this call should not be stored.
  static AuthParts* Get();

  virtual ~AuthParts() = default;

  virtual AuthSessionStorage* GetAuthSessionStorage() = 0;
  virtual AuthHub* GetAuthHub() = 0;
  virtual CryptohomeCore* GetCryptohomeCore() = 0;
  virtual AuthPolicyConnector* GetAuthPolicyConnector() = 0;
  virtual LegacyAuthSurfaceRegistry* GetLegacyAuthSurfaceRegistry() = 0;
  virtual AuthSurfaceRegistry* GetAuthSurfaceRegistry() = 0;

  virtual void RegisterEngineFactory(
      std::unique_ptr<AuthFactorEngineFactory> factory) = 0;
  virtual void RegisterEarlyLoginAuthPolicyConnector(
      std::unique_ptr<AuthPolicyConnector> connector) = 0;
  virtual void ReleaseEarlyLoginAuthPolicyConnector() = 0;
  virtual void SetProfilePrefsAuthPolicyConnector(
      AuthPolicyConnector* connector) = 0;

  virtual const std::vector<std::unique_ptr<AuthFactorEngineFactory>>&
  GetEngineFactories() = 0;

  virtual void Shutdown() = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_PARTS_H_
