// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_STATUS_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_STATUS_CONSUMER_H_

#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthFactorStatusConsumer : public AuthFactorStatusConsumer {
 public:
  MockAuthFactorStatusConsumer();
  MockAuthFactorStatusConsumer(const MockAuthFactorStatusConsumer&) = delete;
  MockAuthFactorStatusConsumer& operator=(const MockAuthFactorStatusConsumer&) =
      delete;
  ~MockAuthFactorStatusConsumer() override;

  MOCK_METHOD(void,
              InitializeUi,
              (AuthFactorsSet, AuthHubConnector*),
              (override));
  MOCK_METHOD(void, OnFactorListChanged, (FactorsStatusMap), (override));
  MOCK_METHOD(void, OnFactorStatusesChanged, (FactorsStatusMap), (override));
  MOCK_METHOD(void, OnFactorCustomSignal, (AshAuthFactor), (override));
  MOCK_METHOD(void, OnFactorAuthFailure, (AshAuthFactor), (override));
  MOCK_METHOD(void, OnFactorAuthSuccess, (AshAuthFactor), (override));
  MOCK_METHOD(void, OnEndAuthentication, (), (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_FACTOR_STATUS_CONSUMER_H_
