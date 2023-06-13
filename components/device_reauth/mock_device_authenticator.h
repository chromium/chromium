// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_MOCK_DEVICE_AUTHENTICATOR_H_
#define COMPONENTS_DEVICE_REAUTH_MOCK_DEVICE_AUTHENTICATOR_H_

#include "base/functional/callback.h"
#include "components/device_reauth/device_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_reauth {

// Mocked DeviceAuthenticator used by unit tests.
class MockDeviceAuthenticator : public DeviceAuthenticator {
 public:
  MockDeviceAuthenticator();

  MOCK_METHOD(bool, CanAuthenticateWithBiometrics, (), (override));
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
  MOCK_METHOD(bool, CanAuthenticateWithBiometricOrScreenLock, (), (override));
#endif
  MOCK_METHOD(void,
              Authenticate,
              (DeviceAuthRequester, AuthenticateCallback, bool),
              (override));
  MOCK_METHOD(void,
              AuthenticateWithMessage,
              (const std::u16string&, AuthenticateCallback),
              (override));
  MOCK_METHOD(void, Cancel, (DeviceAuthRequester), (override));

 private:
  ~MockDeviceAuthenticator() override;
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_MOCK_DEVICE_AUTHENTICATOR_H_
