// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_ATTEMPT_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_ATTEMPT_CONSUMER_H_

#include "chromeos/ash/components/osauth/public/auth_attempt_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthAttemptConsumer : public AuthAttemptConsumer {
 public:
  MockAuthAttemptConsumer();
  MockAuthAttemptConsumer(const MockAuthAttemptConsumer&) = delete;
  MockAuthAttemptConsumer& operator=(const MockAuthAttemptConsumer&) = delete;
  ~MockAuthAttemptConsumer() override;

  MOCK_METHOD(void, OnUserAuthAttemptRejected, (), (override));
  MOCK_METHOD(void,
              OnUserAuthAttemptConfirmed,
              (AuthHubConnector*, raw_ptr<AuthFactorStatusConsumer>&),
              (override));
  MOCK_METHOD(void, OnAccountNotFound, (), (override));
  MOCK_METHOD(void, OnUserAuthAttemptCancelled, (), (override));
  MOCK_METHOD(void, OnFactorAttemptFailed, (AshAuthFactor), (override));
  MOCK_METHOD(void,
              OnUserAuthSuccess,
              (AshAuthFactor, const AuthProofToken&),
              (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_MOCK_AUTH_ATTEMPT_CONSUMER_H_
