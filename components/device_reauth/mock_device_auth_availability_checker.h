// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_MOCK_DEVICE_AUTH_AVAILABILITY_CHECKER_H_
#define COMPONENTS_DEVICE_REAUTH_MOCK_DEVICE_AUTH_AVAILABILITY_CHECKER_H_

#include "components/device_reauth/device_auth_availability_checker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_reauth {

class MockDeviceAuthAvailabilityChecker : public DeviceAuthAvailabilityChecker {
 public:
  MockDeviceAuthAvailabilityChecker();
  ~MockDeviceAuthAvailabilityChecker() override;

  MOCK_METHOD(bool, CanAuthenticateWithBiometrics, (), (override));
  MOCK_METHOD(bool, CanAuthenticateWithBiometricOrScreenLock, (), (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(BiometricStatus, GetBiometricAvailabilityStatus, (), (override));
#endif
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_MOCK_DEVICE_AUTH_AVAILABILITY_CHECKER_H_
