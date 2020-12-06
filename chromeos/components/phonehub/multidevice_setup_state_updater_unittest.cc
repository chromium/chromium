// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/multidevice_setup_state_updater.h"

#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

class MultideviceSetupStateUpdaterTest : public testing::Test {
 protected:
  MultideviceSetupStateUpdaterTest() = default;
  ~MultideviceSetupStateUpdaterTest() override = default;

  MultideviceSetupStateUpdaterTest(const MultideviceSetupStateUpdaterTest&) =
      delete;
  MultideviceSetupStateUpdaterTest& operator=(
      const MultideviceSetupStateUpdaterTest&) = delete;

  // testing::Test:
  void SetUp() override {
    MultideviceSetupStateUpdater::RegisterPrefs(pref_service_.registry());

    updater_ = std::make_unique<MultideviceSetupStateUpdater>(
        &pref_service_, &fake_multidevice_setup_client_,
        &fake_notification_access_manager_);
  }

  void SetNotififcationAccess(bool enabled) {
    fake_notification_access_manager_.SetAccessStatusInternal(
        enabled
            ? NotificationAccessManager::AccessStatus::kAccessGranted
            : NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  }

  void SetFeatureState(Feature feature, FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(feature, feature_state);
  }

  void SetHostStatus(HostStatus host_status) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(host_status, base::nullopt /* host_device */));
  }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return &fake_multidevice_setup_client_;
  }

 private:
  std::unique_ptr<MultideviceSetupStateUpdater> updater_;

  TestingPrefServiceSimple pref_service_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  FakeNotificationAccessManager fake_notification_access_manager_;
};

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub) {
  // Test that there is a call to enable kPhoneHub when host status goes from
  // kHostSetLocallyButWaitingForBackendConfirmation to kHostVerified.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);

  // Test that there is a call to enable kPhoneHub when host status goes from
  // kHostSetLocallyButWaitingForBackendConfirmation to
  // kHostSetButNotYetVerified, then finally to kHostVerified.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostSetButNotYetVerified);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, DisablePhoneHubNotifications) {
  SetNotififcationAccess(true);

  // Test that there is a call to disable kPhoneHubNotifications when
  // notification access has been revoked.
  SetNotififcationAccess(false);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubNotifications,
      /*expected_enabled=*/false, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

}  // namespace phonehub
}  // namespace chromeos
