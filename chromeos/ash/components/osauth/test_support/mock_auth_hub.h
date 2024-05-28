// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_HUB_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_HUB_H_

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthHub : public AuthHub {
 public:
  MockAuthHub();
  MockAuthHub(const MockAuthHub&) = delete;
  MockAuthHub& operator=(const MockAuthHub&) = delete;
  ~MockAuthHub() override;

  MOCK_METHOD(void, InitializeForMode, (AuthHubMode), (override));
  MOCK_METHOD(void, EnsureInitialized, (base::OnceClosure), (override));
  MOCK_METHOD(void,
              StartAuthentication,
              (AccountId, AuthPurpose, AuthAttemptConsumer*),
              (override));
  MOCK_METHOD(void, CancelCurrentAttempt, (AuthHubConnector*), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_HUB_H_
