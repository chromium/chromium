// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include <string>

#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const char kFakePhoneKey[] = "fake-phone-key";
const char kFakePhoneName[] = "Phony Phone";

}  // namespace

class MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest
    : public testing::Test {
 public:
  MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest(
      const MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest&) =
      delete;
  MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest& operator=(
      const MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest&) =
      delete;

 protected:
  MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest() = default;

  ~MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest() override =
      default;

  void SetUp() override {
    fake_android_sms_app_helper_delegate_ =
        std::make_unique<FakeAndroidSmsAppHelperDelegate>();
    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();
    fake_feature_state_manager_ = std::make_unique<FakeFeatureStateManager>();
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    android_sms_app_installing_status_observer_ =
        AndroidSmsAppInstallingStatusObserver::Factory::Create(
            fake_host_status_provider_.get(), fake_feature_state_manager_.get(),
            fake_android_sms_app_helper_delegate_.get(),
            test_pref_service_.get());
  }

  void Initialize() {
    fake_android_sms_app_helper_delegate_->set_is_app_registry_ready(true);
    SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
    SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
    fake_app_helper_delegate()->Reset();
  }

  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
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

  mojom::FeatureState GetMessagesFeatureState() {
    return fake_feature_state_manager_->GetFeatureState(
        mojom::Feature::kMessages);
  }

  FakeAndroidSmsAppHelperDelegate* fake_android_sms_app_helper_delegate() {
    return fake_android_sms_app_helper_delegate_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

 private:
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<FakeFeatureStateManager> fake_feature_state_manager_;
  std::unique_ptr<FakeAndroidSmsAppHelperDelegate>
      fake_android_sms_app_helper_delegate_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;

  std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
      android_sms_app_installing_status_observer_;
};

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsAfterHostPending) {
  Initialize();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      GetFakePhone());
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsAfterHostVerified) {
  Initialize();
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotAllowed) {
  Initialize();
  SetMessagesFeatureState(mojom::FeatureState::kProhibitedByPolicy);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotDisableFeatureIfAppRegistryNotReady) {
  Initialize();
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
  fake_app_helper_delegate()->Reset();
  fake_app_helper_delegate()->set_is_app_registry_ready(false);
  SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_EQ(GetMessagesFeatureState(), mojom::FeatureState::kEnabledByUser);
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallsAfterHostVerifiedIfNotSupportedByPhone) {
  Initialize();
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByPhone);
  fake_app_helper_delegate()->Reset();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
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
                    std::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, GetFakePhone());
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       InstallsWhenFeatureBecomesEnabled) {
  Initialize();
  SetMessagesFeatureState(mojom::FeatureState::kNotSupportedByChromebook);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kEnabledByUser);
  EXPECT_TRUE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       CleansUpPwaInstallationWhenDisabled) {
  Initialize();
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
  Initialize();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kDisabledByUser);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallWhenSuiteIsDisabledByUser) {
  Initialize();
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(mojom::FeatureState::kUnavailableSuiteDisabled);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

TEST_F(MultiDeviceSetupAndroidSmsAppInstallingStatusObserverTest,
       DoesNotInstallIfNotVerified) {
  Initialize();
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
  SetMessagesFeatureState(
      mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts);
  EXPECT_FALSE(fake_app_helper_delegate()->has_installed_app());
}

}  // namespace multidevice_setup

}  // namespace ash
