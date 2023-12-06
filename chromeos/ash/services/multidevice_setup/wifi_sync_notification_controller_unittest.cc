// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/wifi_sync_notification_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/ash/services/multidevice_setup/fake_account_status_change_delegate_notifier.h"
#include "chromeos/ash/services/multidevice_setup/fake_global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 4;

}  // namespace

class MultiDeviceSetupWifiSyncNotificationControllerTest
    : public testing::Test {
 public:
  MultiDeviceSetupWifiSyncNotificationControllerTest(
      const MultiDeviceSetupWifiSyncNotificationControllerTest&) = delete;
  MultiDeviceSetupWifiSyncNotificationControllerTest& operator=(
      const MultiDeviceSetupWifiSyncNotificationControllerTest&) = delete;

 protected:
  MultiDeviceSetupWifiSyncNotificationControllerTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupWifiSyncNotificationControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    SetWifiSyncSupportedInDeviceSyncClient();

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    WifiSyncNotificationController::RegisterPrefs(
        test_pref_service_->registry());
    // Allow Wifi Sync by policy
    test_pref_service_->registry()->RegisterBooleanPref(
        kWifiSyncAllowedPrefName, true);
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);
    fake_account_status_change_delegate_ =
        std::make_unique<FakeAccountStatusChangeDelegate>();
    fake_account_status_change_delegate_notifier_ =
        std::make_unique<FakeAccountStatusChangeDelegateNotifier>();
    fake_account_status_change_delegate_notifier_
        ->SetAccountStatusChangeDelegateRemote(
            fake_account_status_change_delegate_->GenerateRemote());
    fake_account_status_change_delegate_notifier_->FlushForTesting();
    multidevice::RemoteDeviceRef local_device =
        multidevice::CreateRemoteDeviceRefForTest();
    GetMutableRemoteDevice(local_device)
        ->software_features[multidevice::SoftwareFeature::kWifiSyncClient] =
        multidevice::SoftwareFeatureState::kSupported;
    fake_device_sync_client_->set_local_device_metadata(local_device);
  }

  void TearDown() override {}

  void SetHostInDeviceSyncClient(
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    for (const auto& remote_device : test_devices_) {
      bool should_be_host =
          host_device != std::nullopt &&
          ((!remote_device.instance_id().empty() &&
            host_device->instance_id() == remote_device.instance_id()) ||
           (!remote_device.GetDeviceId().empty() &&
            host_device->GetDeviceId() == remote_device.GetDeviceId()));

      GetMutableRemoteDevice(remote_device)
          ->software_features
              [multidevice::SoftwareFeature::kBetterTogetherHost] =
          should_be_host ? multidevice::SoftwareFeatureState::kEnabled
                         : multidevice::SoftwareFeatureState::kSupported;
    }
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetWifiSyncSupportedInDeviceSyncClient() {
    for (const auto& remote_device : test_devices_) {
      GetMutableRemoteDevice(remote_device)
          ->software_features[multidevice::SoftwareFeature::kWifiSyncHost] =
          multidevice::SoftwareFeatureState::kSupported;
    }
  }

  void CreateDelegate(
      const std::optional<multidevice::RemoteDeviceRef>& initial_host) {
    SetHostInDeviceSyncClient(initial_host);
    SetHostWithStatus(initial_host);

    feature_manager_ = std::make_unique<FakeGlobalStateFeatureManager>();
    notification_controller_ = WifiSyncNotificationController::Factory::Create(
        feature_manager_.get(), fake_host_status_provider_.get(),
        test_pref_service_.get(), fake_device_sync_client_.get(),
        fake_account_status_change_delegate_notifier_.get());
  }

  void SetHostWithStatus(
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    mojom::HostStatus host_status =
        (host_device == std::nullopt ? mojom::HostStatus::kNoEligibleHosts
                                     : mojom::HostStatus::kHostVerified);
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
  }

  void SetIsFeatureEnabled(bool enabled) {
    feature_manager_->SetIsFeatureEnabled(enabled);

    HostStatusProvider::HostStatusWithDevice host_with_status =
        fake_host_status_provider_->GetHostWithStatus();
    if (host_with_status.host_status() != mojom::HostStatus::kHostVerified) {
      return;
    }

    multidevice::RemoteDeviceRef host_device = *host_with_status.host_device();

    bool enabled_on_backend =
        (host_device.GetSoftwareFeatureState(
             multidevice::SoftwareFeature::kWifiSyncHost) ==
         multidevice::SoftwareFeatureState::kEnabled);
    bool pending_request_state_same_as_backend =
        (enabled == enabled_on_backend);

    if (pending_request_state_same_as_backend) {
      return;
    }
  }

  FakeHostStatusProvider* fake_host_status_provider() {
    return fake_host_status_provider_.get();
  }

  FakeAccountStatusChangeDelegate* fake_account_status_change_delegate() {
    return fake_account_status_change_delegate_.get();
  }

  void FlushDelegateNotifier() {
    fake_account_status_change_delegate_notifier_->FlushForTesting();
  }

  const multidevice::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

  session_manager::SessionManager* session_manager() {
    return session_manager_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAccountStatusChangeDelegateNotifier>
      fake_account_status_change_delegate_notifier_;
  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;

  std::unique_ptr<GlobalStateFeatureManager> feature_manager_;
  std::unique_ptr<WifiSyncNotificationController> notification_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MultiDeviceSetupWifiSyncNotificationControllerTest,
       Notification_ShownOnFirstUnlockAfterPhoneEnabled) {
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  CreateDelegate(test_devices()[0] /* initial_host */);

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // Simulate lock/unlock
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);

  FlushDelegateNotifier();

  // Shown on first unlock.
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_eligible_for_wifi_sync_events_handled());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);

  FlushDelegateNotifier();

  // Not shown on second unlock.
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_eligible_for_wifi_sync_events_handled());
}

TEST_F(MultiDeviceSetupWifiSyncNotificationControllerTest,
       Notification_NotShownIfAlreadyEnabled) {
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  CreateDelegate(test_devices()[0] /* initial_host */);
  SetIsFeatureEnabled(true);

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // Simulate lock/unlock
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);

  FlushDelegateNotifier();

  EXPECT_EQ(0u, fake_account_status_change_delegate()
                    ->num_eligible_for_wifi_sync_events_handled());
}

}  // namespace multidevice_setup

}  // namespace ash
