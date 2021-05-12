// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_BIOMETRIC_AUTHENTICATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_BIOMETRIC_AUTHENTICATOR_H_

#include "base/callback.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// Mocked BiometricAuthenticator used by unit tests.
class MockBiometricAuthenticator : public BiometricAuthenticator {
 public:
  MockBiometricAuthenticator();

  MOCK_METHOD(BiometricsAvailability, CanAuthenticate, (), (override));
  MOCK_METHOD(void,
              Authenticate,
              (BiometricAuthRequester, AuthenticateCallback),
              (override));
  MOCK_METHOD(void, Cancel, (BiometricAuthRequester), (override));

 private:
  ~MockBiometricAuthenticator() override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_BIOMETRIC_AUTHENTICATOR_H_