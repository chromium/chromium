// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include <string>

#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const char kFakePhoneKey[] = "fake-phone-key";
const char kFakePhoneName[] = "Phony Phone";

}  // namespace

class MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest
    : public testing::Test {
 protected:
  MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest() = default;

  ~MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest() override =
      default;

  void SetUp() override {
    auto fake_android_sms_app_helper_delegate =
        std::make_unique<FakeAndroidSmsAppHelperDelegate>();
    fake_android_sms_app_helper_delegate_ =
        fake_android_sms_app_helper_delegate.get();
    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();
    fake_feature_state_manager_ = std::make_unique<FakeFeatureStateManager>();
    android_sms_app_installing_status_observer_ =
        AndroidSmsAppInstallingStatusObserver::Factory::Get()->BuildInstance(
            fake_host_status_provider_.get(), fake_feature_state_manager_.get(),
            std::move(fake_android_sms_app_helper_delegate));

    SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
    SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
    fake_app_helper_delegate()->Reset();
    EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
  }

  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
  }

  FakeAndroidSmsAppHelperDelegate* fake_app_helper_delegate() {
    return fake_android_sms_app_helper_delegate_;
  }

  cryptauth::RemoteDeviceRef GetFakePhone() {
    return cryptauth::RemoteDeviceRefBuilder()
        .SetPublicKey(kFakePhoneKey)
        .SetName(kFakePhoneName)
        .Build();
  }

  void SetMessagesFeatureState(mojom::FeatureState feature_state) {
    fake_feature_state_manager_->SetFeatureState(mojom::Feature::kMessages,
                                                 feature_state);
  }

 private:
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<FakeFeatureStateManager> fake_feature_state_manager_;
  FakeAndroidSmsAppHelperDelegate* fake_android_sms_app_helper_delegate_;

  std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
      android_sms_app_installing_status_observer_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest);
};

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsAfterHostPending) {
  SetMessagesFeatureState(mojom::FeatureState::kUnavailableNoVerifiedHost);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      GetFakePhone());
  EXPECT_TRUE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsAfterHostVerified) {
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_TRUE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotAllowed) {
  SetMessagesFeatureState(mojom::FeatureState::kProhibitedByPolicy);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotSupportedByPhone) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByPhone);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotSupportedByChromebook) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByChromebook);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsWhenFeatureBecomesEnabled) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByChromebook);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
  SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
  EXPECT_TRUE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsEvenIfFeatureIsDisabledByUser) {
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
  SetMessagesFeatureState(mojom::FeatureState::kDisabledByUser);
  EXPECT_TRUE(fake_app_helper_delegate()->HasInstalledApp());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallIfNotVerified) {
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
  SetMessagesFeatureState(mojom::FeatureState::kUnavailableNoVerifiedHost);
  EXPECT_FALSE(fake_app_helper_delegate()->HasInstalledApp());
}

}  // namespace multidevice_setup

}  // namespace chromeos
