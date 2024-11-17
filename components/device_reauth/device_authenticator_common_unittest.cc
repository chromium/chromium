// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_reauth/device_authenticator_common.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/device_reauth/device_authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Implementation of DeviceAuthenticatorCommon for testing.
class FakeDeviceAuthenticatorCommon : public DeviceAuthenticatorCommon {
 public:
  using DeviceAuthenticatorCommon::NeedsToAuthenticate;
  using DeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful;

  FakeDeviceAuthenticatorCommon(DeviceAuthenticatorProxy* proxy,
                                base::TimeDelta auth_validity_period);
  ~FakeDeviceAuthenticatorCommon() override;

  bool CanAuthenticateWithBiometrics() override;

  bool CanAuthenticateWithBiometricOrScreenLock() override;

  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

#if BUILDFLAG(IS_ANDROID)
  device_reauth::BiometricStatus GetBiometricAvailabilityStatus() override;
#endif

  void Cancel() override;
};

FakeDeviceAuthenticatorCommon::FakeDeviceAuthenticatorCommon(
    DeviceAuthenticatorProxy* proxy,
    base::TimeDelta auth_validity_period)
    : DeviceAuthenticatorCommon(proxy, auth_validity_period, "") {}

FakeDeviceAuthenticatorCommon::~FakeDeviceAuthenticatorCommon() = default;

bool FakeDeviceAuthenticatorCommon::CanAuthenticateWithBiometrics() {
  NOTIMPLEMENTED();
  return false;
}

bool FakeDeviceAuthenticatorCommon::CanAuthenticateWithBiometricOrScreenLock() {
  NOTIMPLEMENTED();
  return false;
}

void FakeDeviceAuthenticatorCommon::Cancel() {
  NOTIMPLEMENTED();
}

void FakeDeviceAuthenticatorCommon::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_ANDROID)
device_reauth::BiometricStatus
FakeDeviceAuthenticatorCommon::GetBiometricAvailabilityStatus() {
  NOTIMPLEMENTED();
  return device_reauth::BiometricStatus::kUnavailable;
}
#endif  // BUILDFLAG(IS_ANDROID)

constexpr base::TimeDelta kAuthValidityPeriod = base::Seconds(60);

}  // namespace

class DeviceAuthenticatorCommonTest : public testing::Test {
 public:
  void SetUp() override {
    proxy_ = std::make_unique<DeviceAuthenticatorProxy>();
    other_proxy_ = std::make_unique<DeviceAuthenticatorProxy>();

    // Simulates platform specific DeviceAuthenticator received from the
    // factory.
    authenticator_pointer_ = std::make_unique<FakeDeviceAuthenticatorCommon>(
        proxy_.get(), kAuthValidityPeriod);
    authenticator_pointer_other_profile_ =
        std::make_unique<FakeDeviceAuthenticatorCommon>(other_proxy_.get(),
                                                        kAuthValidityPeriod);
  }

  DeviceAuthenticatorProxy* proxy() { return proxy_.get(); }

  FakeDeviceAuthenticatorCommon* authenticator_pointer() {
    return authenticator_pointer_.get();
  }

  FakeDeviceAuthenticatorCommon* authenticator_pointer_other_profile() {
    return authenticator_pointer_other_profile_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<DeviceAuthenticatorProxy> proxy_;
  std::unique_ptr<DeviceAuthenticatorProxy> other_proxy_;
  std::unique_ptr<FakeDeviceAuthenticatorCommon> authenticator_pointer_;
  std::unique_ptr<FakeDeviceAuthenticatorCommon>
      authenticator_pointer_other_profile_;
};

// Checks if user can perform an operation without reauthenticating during
// `kAuthValidityPeriod` since previous authentication. And if needs to
// authenticate after that time.
// Also checks that other profiles need to authenticate.
TEST_F(DeviceAuthenticatorCommonTest, NeedAuthentication) {
  authenticator_pointer()->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);

  task_environment().FastForwardBy(kAuthValidityPeriod / 2);
  EXPECT_FALSE(authenticator_pointer()->NeedsToAuthenticate());
  EXPECT_TRUE(authenticator_pointer_other_profile()->NeedsToAuthenticate());

  task_environment().FastForwardBy(kAuthValidityPeriod);
  EXPECT_TRUE(authenticator_pointer()->NeedsToAuthenticate());
}

// Checks that user cannot perform an operation without reauthenticating when
// `kAuthValidityPeriod` is 0.
TEST_F(DeviceAuthenticatorCommonTest, NeedAuthenticationImmediately) {
  auto authenticator_pointer_0_seconds =
      std::make_unique<FakeDeviceAuthenticatorCommon>(proxy(),
                                                      base::Seconds(0));

  authenticator_pointer_0_seconds->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);
  EXPECT_TRUE(authenticator_pointer_0_seconds->NeedsToAuthenticate());
}
