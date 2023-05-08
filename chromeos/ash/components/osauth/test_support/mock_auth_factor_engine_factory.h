// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_ENGINE_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_ENGINE_FACTORY_H_

#include <memory>

#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthFactorEngineFactory : public AuthFactorEngineFactory {
 public:
  MockAuthFactorEngineFactory();
  MockAuthFactorEngineFactory(const MockAuthFactorEngineFactory&) = delete;
  MockAuthFactorEngineFactory& operator=(const MockAuthFactorEngineFactory&) =
      delete;
  ~MockAuthFactorEngineFactory() override;

  MOCK_METHOD(AshAuthFactor, GetFactor, (), (override));
  MOCK_METHOD(std::unique_ptr<AuthFactorEngine>,
              CreateEngine,
              (AuthHubMode),
              (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_ENGINE_FACTORY_H_
