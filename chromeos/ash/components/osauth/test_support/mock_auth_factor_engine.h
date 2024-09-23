// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_ENGINE_H_

#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthFactorEngine : public AuthFactorEngine {
 public:
  MockAuthFactorEngine();
  MockAuthFactorEngine(const MockAuthFactorEngine&) = delete;
  MockAuthFactorEngine& operator=(const MockAuthFactorEngine&) = delete;
  ~MockAuthFactorEngine() override;

  MOCK_METHOD(AshAuthFactor, GetFactor, (), (const, override));
  MOCK_METHOD(void, InitializeCommon, (CommonInitCallback), (override));
  MOCK_METHOD(void, ShutdownCommon, (ShutdownCallback), (override));
  MOCK_METHOD(void,
              StartAuthFlow,
              (const AccountId&, AuthPurpose, FactorEngineObserver*),
              (override));
  MOCK_METHOD(void, UpdateObserver, (FactorEngineObserver*), (override));
  MOCK_METHOD(void, CleanUp, (CleanupCallback), (override));
  MOCK_METHOD(void, StopAuthFlow, (ShutdownCallback), (override));
  MOCK_METHOD(AuthProofToken, StoreAuthenticationContext, (), (override));
  MOCK_METHOD(void, SetUsageAllowed, (UsageAllowed), (override));
  MOCK_METHOD(bool, IsDisabledByPolicy, (), (override));
  MOCK_METHOD(bool, IsLockedOut, (), (override));
  MOCK_METHOD(bool, IsFactorSpecificRestricted, (), (override));
  MOCK_METHOD(void, InitializationTimedOut, (), (override));
  MOCK_METHOD(void, ShutdownTimedOut, (), (override));
  MOCK_METHOD(void, StartFlowTimedOut, (), (override));
  MOCK_METHOD(void, StopFlowTimedOut, (), (override));
};

class MockAuthFactorEngineObserver
    : public AuthFactorEngine::FactorEngineObserver {
 public:
  MockAuthFactorEngineObserver();
  MockAuthFactorEngineObserver(const MockAuthFactorEngineObserver&) = delete;
  MockAuthFactorEngineObserver& operator=(const MockAuthFactorEngineObserver&) =
      delete;
  ~MockAuthFactorEngineObserver() override;

  MOCK_METHOD(void, OnFactorPresenceChecked, (AshAuthFactor, bool), (override));
  MOCK_METHOD(void, OnFactorAttempt, (AshAuthFactor), (override));
  MOCK_METHOD(void, OnFactorAttemptResult, (AshAuthFactor, bool), (override));

  MOCK_METHOD(void, OnPolicyChanged, (AshAuthFactor), (override));
  MOCK_METHOD(void, OnLockoutChanged, (AshAuthFactor), (override));
  MOCK_METHOD(void,
              OnFactorSpecificRestrictionsChanged,
              (AshAuthFactor),
              (override));
  MOCK_METHOD(void, OnCriticalError, (AshAuthFactor), (override));
  MOCK_METHOD(void, OnFactorCustomSignal, (AshAuthFactor), (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_ENGINE_H_
