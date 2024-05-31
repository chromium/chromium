// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_HUB_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_HUB_CONNECTOR_H_

#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthHubConnector : public AuthHubConnector {
 public:
  MockAuthHubConnector();
  MockAuthHubConnector(const MockAuthHubConnector&) = delete;
  MockAuthHubConnector& operator=(const MockAuthHubConnector&) = delete;
  ~MockAuthHubConnector() override;

  MOCK_METHOD(AuthFactorEngine*, GetEngine, (AshAuthFactor), (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_HUB_CONNECTOR_H_
