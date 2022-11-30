// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_MOCK_BIOMETRIC_AUTHENTICATOR_H_
#define COMPONENTS_DEVICE_REAUTH_MOCK_BIOMETRIC_AUTHENTICATOR_H_

#include "base/callback.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_reauth {

// Mocked BiometricAuthenticator used by unit tests.
class MockBiometricAuthenticator : public BiometricAuthenticator {
 public:
  MockBiometricAuthenticator();

  MOCK_METHOD(bool, CanAuthenticate, (BiometricAuthRequester), (override));
  MOCK_METHOD(void,
              Authenticate,
              (BiometricAuthRequester, AuthenticateCallback, bool),
              (override));
  MOCK_METHOD(void,
              AuthenticateWithMessage,
              (BiometricAuthRequester,
               const std::u16string&,
               AuthenticateCallback),
              (override));
  MOCK_METHOD(void, Cancel, (BiometricAuthRequester), (override));

 private:
  ~MockBiometricAuthenticator() override;
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_MOCK_BIOMETRIC_AUTHENTICATOR_H_
