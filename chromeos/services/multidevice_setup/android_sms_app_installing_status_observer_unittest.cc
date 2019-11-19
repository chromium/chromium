// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include <string>

#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
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
    fake_android_sms_app_helper_delegate_ =
        std::make_unique<FakeAndroidSmsAppHelperDelegate>();
    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();
    fake_feature_state_manager_ = std::make_unique<FakeFeatureStateManager>();
    android_sms_app_installing_status_observer_ =
        AndroidSmsAppInstallingStatusObserver::Factory::Get()->BuildInstance(
            fake_host_status_provider_.get(), fake_feature_state_manager_.get(),
            fake_android_sms_app_helper_delegate_.get());

    SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
    SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
    fake_app_helper_delegate()->Reset();
    EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  }

  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDeviceRef>& host_device) {
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
  }

  FakeAndroidSmsAppHelperDelegate* fake_app_helper_delegate() {
    return fake_android_sms_app_helper_delegate_.get();
  }

  multidevice::RemoteDeviceRef GetFakePhone() {
    return multidevice::RemoteDeviceRefBuilder()
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
  std::unique_ptr<FakeAndroidSmsAppHelperDelegate>
      fake_android_sms_app_helper_delegate_;

  std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
      android_sms_app_installing_status_observer_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest);
};

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsAfterHostPending) {
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      GetFakePhone());
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsAfterHostVerified) {
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotAllowed) {
  SetMessagesFeatureState(mojom::FeatureState::kProhibitedByPolicy);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallAfterHostVerifiedIfUninstalledByUser) {
  fake_app_helper_delegate()->Reset();
  fake_app_helper_delegate()->set_has_app_been_manually_uninstalled(true);

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotSupportedByPhone) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByPhone);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotSupportedByChromebook) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByChromebook);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsWhenFeatureBecomesEnabled) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByChromebook);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       CleansUpPwaInstallationWhenDisabled) {
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByChromebook);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
  EXPECT_TRUE(fake_app_helper_delegate()->is_default_to_persist_cookie_set());

  SetMessagesFeatureState(mojom::FeatureState::kDisabledByUser);
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
  EXPECT_FALSE(fake_app_helper_delegate()->is_default_to_persist_cookie_set());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallWhenFeatureIsDisabledByUser) {
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kDisabledByUser);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallWhenSuiteIsDisabledByUser) {
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallIfNotVerified) {
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kUnavailableNoVerifiedHost);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

}  // namespace multidevice_setup

}  // namespace chromeos
