// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/wifi_sync_feature_manager_impl.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "base/unguessable_token.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const char kPendingWifiSyncRequestEnabledPrefName[] =
    "multidevice_setup.pending_set_wifi_sync_enabled_request";

enum PendingState { PendingNone = 0, PendingEnable = 1, PendingDisable = 2 };

const size_t kNumTestDevices = 4;

}  // namespace

class MultiDeviceSetupWifiSyncFeatureManagerImplTest
    : public ::testing::TestWithParam<bool> {
 protected:
  MultiDeviceSetupWifiSyncFeatureManagerImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupWifiSyncFeatureManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    SetFeatureFlags(GetParam() /* use_v1_devicesync */);

    // Tests are run once to simulate when v1 DeviceSync is enabled and once to
    // simulate when it is disabled, leaving only v2 DeviceSync operational. In
    // the former case, only public keys are needed, and in the latter case,
    // only Instance IDs are needed.
    for (multidevice::RemoteDeviceRef device : test_devices_) {
      if (features::ShouldUseV1DeviceSync())
        GetMutableRemoteDevice(device)->instance_id.clear();
      else
        GetMutableRemoteDevice(device)->public_key.clear();
    }

    SetWifiSyncSupportedInDeviceSyncClient();

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    WifiSyncFeatureManagerImpl::RegisterPrefs(test_pref_service_->registry());

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);
  }

  void TearDown() override {}

  void SetHostInDeviceSyncClient(
      const base::Optional<multidevice::RemoteDeviceRef>& host_device) {
    for (const auto& remote_device : test_devices_) {
      bool should_be_host =
          host_device != base::nullopt &&
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
      const base::Optional<multidevice::RemoteDeviceRef>& initial_host,
      int initial_pending_wifi_sync_request = PendingNone) {
    SetHostInDeviceSyncClient(initial_host);
    test_pref_service_->SetInteger(kPendingWifiSyncRequestEnabledPrefName,
                                   initial_pending_wifi_sync_request);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    SetHostWithStatus(initial_host);

    delegate_ = WifiSyncFeatureManagerImpl::Factory::Create(
        fake_host_status_provider_.get(), test_pref_service_.get(),
        fake_device_sync_client_.get(), std::move(mock_timer));
  }

  void SetHostWithStatus(
      const base::Optional<multidevice::RemoteDeviceRef>& host_device) {
    mojom::HostStatus host_status =
        (host_device == base::nullopt ? mojom::HostStatus::kNoEligibleHosts
                                      : mojom::HostStatus::kHostVerified);
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
  }

  void SetIsWifiSyncEnabled(bool enabled) {
    delegate_->SetIsWifiSyncEnabled(enabled);

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

    VerifyLatestSetWifiSyncHostNetworkRequest(host_device, enabled);
  }

  void VerifyLatestSetWifiSyncHostNetworkRequest(
      const multidevice::RemoteDeviceRef expected_host,
      bool expected_should_enable) {
    if (features::ShouldUseV1DeviceSync()) {
      ASSERT_FALSE(
          fake_device_sync_client_->set_software_feature_state_inputs_queue()
              .empty());
      const device_sync::FakeDeviceSyncClient::SetSoftwareFeatureStateInputs&
          inputs = fake_device_sync_client_
                       ->set_software_feature_state_inputs_queue()
                       .back();
      EXPECT_EQ(expected_host.public_key(), inputs.public_key);
      EXPECT_EQ(multidevice::SoftwareFeature::kWifiSyncHost,
                inputs.software_feature);
      EXPECT_EQ(expected_should_enable, inputs.enabled);
      EXPECT_EQ(expected_should_enable, inputs.is_exclusive);
      return;
    }

    // Verify inputs to SetFeatureStatus().
    ASSERT_FALSE(
        fake_device_sync_client_->set_feature_status_inputs_queue().empty());
    const device_sync::FakeDeviceSyncClient::SetFeatureStatusInputs& inputs =
        fake_device_sync_client_->set_feature_status_inputs_queue().back();
    EXPECT_EQ(expected_host.instance_id(), inputs.device_instance_id);
    EXPECT_EQ(multidevice::SoftwareFeature::kWifiSyncHost, inputs.feature);
    EXPECT_EQ(expected_should_enable
                  ? device_sync::FeatureStatusChange::kEnableExclusively
                  : device_sync::FeatureStatusChange::kDisable,
              inputs.status_change);
  }

  int GetSetHostNetworkRequestCallbackQueueSize() {
    return features::ShouldUseV1DeviceSync()
               ? fake_device_sync_client_
                     ->GetSetSoftwareFeatureStateInputsQueueSize()
               : fake_device_sync_client_->GetSetFeatureStatusInputsQueueSize();
  }

  void InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult result_code,
      bool expected_to_notify_observer_and_start_retry_timer) {
    if (features::ShouldUseV1DeviceSync()) {
      fake_device_sync_client_->InvokePendingSetSoftwareFeatureStateCallback(
          result_code);
    } else {
      fake_device_sync_client_->InvokePendingSetFeatureStatusCallback(
          result_code);
    }

    EXPECT_EQ(expected_to_notify_observer_and_start_retry_timer,
              mock_timer_->IsRunning());
  }

  void SetWifiSyncHostInDeviceSyncClient(
      const base::Optional<multidevice::RemoteDeviceRef>& host_device,
      bool enabled) {
    GetMutableRemoteDevice(*host_device)
        ->software_features[multidevice::SoftwareFeature::kWifiSyncHost] =
        (enabled ? multidevice::SoftwareFeatureState::kEnabled
                 : multidevice::SoftwareFeatureState::kSupported);
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  FakeHostStatusProvider* fake_host_status_provider() {
    return fake_host_status_provider_.get();
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() {
    return fake_device_sync_client_.get();
  }

  base::MockOneShotTimer* mock_timer() { return mock_timer_; }

  WifiSyncFeatureManager* delegate() { return delegate_.get(); }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

  const multidevice::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

 private:
  void SetFeatureFlags(bool use_v1_devicesync) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    // These flags have no direct effect of on the host backend delegate;
    // however, v2 Enrollment and DeviceSync must be enabled before v1
    // DeviceSync can be disabled.
    enabled_features.push_back(chromeos::features::kCryptAuthV2Enrollment);
    enabled_features.push_back(chromeos::features::kCryptAuthV2DeviceSync);

    if (use_v1_devicesync) {
      disabled_features.push_back(
          chromeos::features::kDisableCryptAuthV1DeviceSync);
    } else {
      enabled_features.push_back(
          chromeos::features::kDisableCryptAuthV1DeviceSync);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  base::MockOneShotTimer* mock_timer_;

  std::unique_ptr<WifiSyncFeatureManager> delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupWifiSyncFeatureManagerImplTest);
};

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest, Success) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable wifi sync on host device and succeed
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kEnabled);

  // Attempt to disable wifi sync on host device and succeed
  SetIsWifiSyncEnabled(false);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kEnabled);

  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], false /* enabled */);
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest, Failure) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable wifi sync on host device and fail
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // A retry should have been scheduled, so fire the timer to start the retry.
  mock_timer()->Fire();

  // Simulate another failure.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       MultipleRequests_FirstFail_ThenSucceed) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable wifi sync on host device and fail
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // The retry timer is running; however, instead of relying on that, call
  // SetIsWifiSyncEnabled() again to trigger an immediate
  // retry without the timer.
  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       PendingRequest_NoSyncedHostDevice) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable wifi sync on test_device 0
  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // Fail to set wifi sync on test_device 0
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
  EXPECT_TRUE(mock_timer()->IsRunning());

  // Remove synced device. This should remove the pending request and stop the
  // retry timer.
  SetHostInDeviceSyncClient(base::nullopt);
  SetHostWithStatus(base::nullopt);
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       InitialPendingEnableRequest_NoInitialDevice) {
  CreateDelegate(base::nullopt /* initial_host */,
                 PendingEnable /* initial_pending_wifi_sync_request_enabled */);

  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       InitialPendingEnableRequest_Success) {
  CreateDelegate(test_devices()[0] /* initial_host */,
                 PendingEnable /* initial_pending_wifi_sync_request_enabled */);

  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       MultiplePendingRequests_EnableDisable) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable->disable->enable wifi sync without invoking any
  // callbacks.
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // Wifi sync is already disabled on back-end so there should be no new pending
  // request
  SetIsWifiSyncEnabled(false);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       PendingRequest_SyncedHostBecomesUnverified) {
  CreateDelegate(test_devices()[0] /* initial_host */,
                 PendingEnable /* initial_pending_wifi_sync_request_enabled */);

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);

  EXPECT_EQ(
      test_pref_service()->GetInteger(kPendingWifiSyncRequestEnabledPrefName),
      PendingNone);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       Retrying_SyncedHostBecomesUnverified) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(mock_timer()->IsRunning());

  // Host becomes unverified, this should stop timer and clear pending request
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  EXPECT_EQ(
      test_pref_service()->GetInteger(kPendingWifiSyncRequestEnabledPrefName),
      PendingNone);
  EXPECT_FALSE(mock_timer()->IsRunning());
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       FailureCallback_SyncedHostBecomesUnverified) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // Set host unverified. This should reset pending request.
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  EXPECT_EQ(
      test_pref_service()->GetInteger(kPendingWifiSyncRequestEnabledPrefName),
      PendingNone);

  // Invoke failure callback. No retry should be scheduled.
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(mock_timer()->IsRunning());
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       NoVerifiedHost_AttemptToEnable) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);

  // Attempt to enable wifi sync on host device
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsWifiSyncEnabled(true);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       StatusChangedOnRemoteDevice) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);

  // Simulate enabled on a remote device.
  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  EXPECT_TRUE(delegate()->IsWifiSyncEnabled());
}

TEST_P(MultiDeviceSetupWifiSyncFeatureManagerImplTest,
       SimultaneousRequests_StartOff_ToggleOnOff) {
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable
  SetIsWifiSyncEnabled(true);
  // Attempt to disable
  SetIsWifiSyncEnabled(false);

  // Only one network request should be in flight at a time
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());

  // Successfully enable on host
  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kEnabled);

  // A new network request should be scheduled to disable
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  SetWifiSyncHostInDeviceSyncClient(test_devices()[0], false /* enabled */);
  InvokePendingSetWifiSyncHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_FALSE(delegate()->IsWifiSyncEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(
                multidevice::SoftwareFeature::kWifiSyncHost),
            multidevice::SoftwareFeatureState::kSupported);
}

// Runs tests twice; once with v1 DeviceSync enabled and once with it disabled.
// TODO(https://crbug.com/1019206): Remove when v1 DeviceSync is disabled,
// when all devices should have an Instance ID.
INSTANTIATE_TEST_SUITE_P(All,
                         MultiDeviceSetupWifiSyncFeatureManagerImplTest,
                         ::testing::Bool());

}  // namespace multidevice_setup

}  // namespace chromeos
