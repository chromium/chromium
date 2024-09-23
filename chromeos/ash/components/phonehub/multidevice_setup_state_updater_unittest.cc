// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/multidevice_setup_state_updater.h"

#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
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
    multidevice_setup::RegisterFeaturePrefs(pref_service_.registry());

    // Set the host status and feature state to realistic default values used
    // during start-up.
    SetFeatureState(Feature::kPhoneHub,
                    FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts);
    SetHostStatus(HostStatus::kNoEligibleHosts);
  }

  void CreateUpdater() {
    updater_ = std::make_unique<MultideviceSetupStateUpdater>(
        &pref_service_, &fake_multidevice_setup_client_,
        &fake_multidevice_feature_access_manager_);
  }

  void DestroyUpdater() { updater_.reset(); }

  void SetNotificationAccess(bool enabled) {
    fake_multidevice_feature_access_manager_
        .SetNotificationAccessStatusInternal(
            enabled
                ? MultideviceFeatureAccessManager::AccessStatus::kAccessGranted
                : MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted,
            MultideviceFeatureAccessManager::AccessProhibitedReason::kUnknown);
  }

  void SetCameraRollAccess(bool enabled) {
    fake_multidevice_feature_access_manager_.SetCameraRollAccessStatusInternal(
        enabled ? MultideviceFeatureAccessManager::AccessStatus::kAccessGranted
                : MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted);
  }

  void SetFeatureState(Feature feature, FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(feature, feature_state);
  }

  void SetHostStatus(HostStatus host_status) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(host_status, std::nullopt /* host_device */));
  }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return &fake_multidevice_setup_client_;
  }

  void SetFeatureEnabledState(const std::string& pref_name, bool enabled) {
    pref_service_.SetBoolean(pref_name, enabled);
  }

  void ForceNotifyNotificationAccessChanged() {
    fake_multidevice_feature_access_manager_.NotifyNotificationAccessChanged();
  }

  void ForceNotifyCameraRollAccessChanged() {
    fake_multidevice_feature_access_manager_.NotifyCameraRollAccessChanged();
  }

 private:

  TestingPrefServiceSimple pref_service_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  FakeMultideviceFeatureAccessManager fake_multidevice_feature_access_manager_;
  std::unique_ptr<MultideviceSetupStateUpdater> updater_;
};

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub) {
  CreateUpdater();

  // Test that there is a call to enable kPhoneHub--if it is currently
  // disabled--when host status goes from
  // kHostSetLocallyButWaitingForBackendConfirmation to kHostVerified.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub_SetButNotVerified) {
  CreateUpdater();

  // Test that there is a call to enable kPhoneHub when host status goes from
  // kHostSetLocallyButWaitingForBackendConfirmation to
  // kHostSetButNotYetVerified, then finally to kHostVerified, when the feature
  // is currently disabled.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostSetButNotYetVerified);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       EnablePhoneHub_WaitForDisabledStateBeforeEnabling) {
  CreateUpdater();

  // After the host is verified, ensure that we wait until the feature state is
  // "disabled" before enabling the feature. We don't want to go from
  // kNotSupportedByPhone to enabled, for instance.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kNotSupportedByPhone);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       EnablePhoneHub_DisabledStateSetBeforeVerification) {
  CreateUpdater();

  // Much like EnablePhoneHub_WaitForDisabledStateBeforeEnabling, but here we
  // test that the order of the feature being set to disabled and the host being
  // verified does not matter.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kNotSupportedByPhone);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       EnablePhoneHub_ReenableAfterMultideviceSetup) {
  CreateUpdater();

  // The user has a verified host phone, but chose to disable the Phone Hub
  // toggle in Settings.
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  // The user disconnects the phone from the multi-device suite.
  SetHostStatus(HostStatus::kEligibleHostExistsButNoHostSet);

  // The user goes through multi-device setup again.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);

  // The Phone Hub feature is automatically re-enabled.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub_PersistIntentToEnable) {
  CreateUpdater();

  // Indicate intent to enable Phone Hub after host verification.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);

  // Simulate the user logging out and back in, for instance. And even though
  // some transient default values are set for the host status and feature
  // state, we should preserve the intent to enable Phone Hub.
  DestroyUpdater();
  SetFeatureState(Feature::kPhoneHub,
                  FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts);
  SetHostStatus(HostStatus::kNoEligibleHosts);
  CreateUpdater();

  // The host status and feature state update to expected values.
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  // The Phone Hub feature is expected to be enabled.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(
    MultideviceSetupStateUpdaterTest,
    EnablePhoneHub_PersistIntentToEnable_HandleTransientHostOrFeatureStates) {
  CreateUpdater();

  // Indicate intent to enable Phone Hub after host verification.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);

  // Simulate the user logging out and back in.
  DestroyUpdater();
  CreateUpdater();

  // Make sure to ignore transient updates after start-up. In other words,
  // maintain our intent to enable Phone Hub after verification.
  SetFeatureState(Feature::kPhoneHub,
                  FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts);
  SetHostStatus(HostStatus::kNoEligibleHosts);

  // The host status and feature state update to expected values.
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  // The Phone Hub feature is expected to be enabled.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, RevokePhoneHubNotificationsAccess) {
  SetNotificationAccess(true);
  CreateUpdater();

  // Test that there is a call to disable kPhoneHubNotifications when
  // notification access has been revoked.
  SetNotificationAccess(false);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubNotifications,
      /*expected_enabled=*/false, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, InvokePhoneHubNotificationsAccess) {
  SetNotificationAccess(false);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  SetFeatureState(Feature::kPhoneHubNotifications,
                  FeatureState::kDisabledByUser);
  CreateUpdater();

  // Test that there is a call to enable kPhoneHubNotifications when
  // notification access has been invoked.
  SetNotificationAccess(true);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubNotifications,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       InitiallyEnablePhoneHubNotifications_OnlyEnableFromDefaultState) {
  SetNotificationAccess(true);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  CreateUpdater();

  // If the notifications feature has not been explicitly set yet, enable it
  // when Phone Hub is enabled and access has been granted.
  ForceNotifyNotificationAccessChanged();
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubNotifications,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       ShouldNotEnablePhoneHubNotificationsIfFeatureIsNotDefaultState) {
  SetNotificationAccess(true);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  // Simulate the notifications feature has been changed by user.
  SetFeatureEnabledState(
      ash::multidevice_setup::kPhoneHubNotificationsEnabledPrefName, false);
  CreateUpdater();

  // We take no action after access is granted because the Phone Hub
  // notifications feature state was already explicitly set; we respect the
  // user's choice.
  ForceNotifyNotificationAccessChanged();
  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

TEST_F(MultideviceSetupStateUpdaterTest,
       InitiallyEnableCameraRoll_OnlyEnableFromDefaultState) {
  SetCameraRollAccess(true);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  CreateUpdater();

  // If the camera roll feature has not been explicitly set yet, enable it
  // when Phone Hub is enabled and access has been granted.
  ForceNotifyCameraRollAccessChanged();
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubCameraRoll,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       ShouldNotEnableCameraRollIfFeatureIsNotDefaultState) {
  SetCameraRollAccess(true);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  // Simulate the camera roll feature has been changed by user.
  SetFeatureEnabledState(
      ash::multidevice_setup::kPhoneHubCameraRollEnabledPrefName, false);
  CreateUpdater();

  // We take no action after access is granted because the camera roll
  // feature state was already explicitly set; we respect the user's choice.
  ForceNotifyCameraRollAccessChanged();
  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

TEST_F(MultideviceSetupStateUpdaterTest,
       InitiallyEnableCameraRoll_DisablePhoneHub) {
  SetCameraRollAccess(false);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);

  // Explicitly disable Phone Hub, all sub feature should be disabled
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  CreateUpdater();

  // No action after access is granted
  SetCameraRollAccess(true);
  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

TEST_F(MultideviceSetupStateUpdaterTest, RevokePhoneHubCameraRollAccess) {
  SetCameraRollAccess(true);
  CreateUpdater();

  // Test that there is a call to disable kPhoneHubCameraRoll when camera roll
  // access has been revoked.
  SetCameraRollAccess(false);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubCameraRoll,
      /*expected_enabled=*/false, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, InvokePhoneHubCameraRollAccess) {
  SetCameraRollAccess(false);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  SetFeatureState(Feature::kPhoneHubCameraRoll, FeatureState::kDisabledByUser);
  CreateUpdater();

  // Test that there is a call to enable kPhoneHubCameraRoll when camera roll
  // access has been invoked.
  SetCameraRollAccess(true);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubCameraRoll,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

}  // namespace phonehub
}  // namespace ash
