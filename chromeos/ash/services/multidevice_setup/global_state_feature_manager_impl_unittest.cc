// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/multidevice_setup/wifi_sync_notification_controller.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const GlobalStateFeatureManagerImpl::Factory::Option kTestOption =
    GlobalStateFeatureManagerImpl::Factory::Option::kWifiSync;
const multidevice::SoftwareFeature kTestHostFeature =
    multidevice::SoftwareFeature::kWifiSyncHost;
const multidevice::SoftwareFeature kTestClientFeature =
    multidevice::SoftwareFeature::kWifiSyncClient;
const std::string& kFeatureAllowedPrefName = kWifiSyncAllowedPrefName;
const char kPendingStatePrefName[] =
    "multidevice_setup.pending_set_wifi_sync_enabled_request";
const base::Feature& kTestFeatureFlag = features::kWifiSyncAndroid;

enum PendingState {
  kPendingNone = 0,
  kPendingEnable = 1,
  kPendingDisable = 2,
  kSetPendingEnableOnVerify = 3
};

const size_t kNumTestDevices = 4;

}  // namespace

class MultiDeviceSetupGlobalStateFeatureManagerImplTest
    : public ::testing::TestWithParam<bool> {
 public:
  MultiDeviceSetupGlobalStateFeatureManagerImplTest(
      const MultiDeviceSetupGlobalStateFeatureManagerImplTest&) = delete;
  MultiDeviceSetupGlobalStateFeatureManagerImplTest& operator=(
      const MultiDeviceSetupGlobalStateFeatureManagerImplTest&) = delete;

 protected:
  MultiDeviceSetupGlobalStateFeatureManagerImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupGlobalStateFeatureManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
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

    SetFeatureSupportedInDeviceSyncClient();

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    GlobalStateFeatureManagerImpl::RegisterPrefs(
        test_pref_service_->registry());
    WifiSyncNotificationController::RegisterPrefs(
        test_pref_service_->registry());
    test_pref_service_->registry()->RegisterBooleanPref(kFeatureAllowedPrefName,
                                                        true);
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);
    multidevice::RemoteDeviceRef local_device =
        multidevice::CreateRemoteDeviceRefForTest();
    GetMutableRemoteDevice(local_device)
        ->software_features[kTestClientFeature] =
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

  void SetFeatureSupportedInDeviceSyncClient() {
    for (const auto& remote_device : test_devices_) {
      GetMutableRemoteDevice(remote_device)
          ->software_features[kTestHostFeature] =
          multidevice::SoftwareFeatureState::kSupported;
    }
  }

  void CreateDelegate(
      const std::optional<multidevice::RemoteDeviceRef>& initial_host,
      int initial_pending_state = kPendingNone) {
    SetHostInDeviceSyncClient(initial_host);
    test_pref_service_->SetInteger(kPendingStatePrefName,
                                   initial_pending_state);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    SetHostWithStatus(initial_host);

    delegate_ = GlobalStateFeatureManagerImpl::Factory::Create(
        kTestOption, fake_host_status_provider_.get(), test_pref_service_.get(),
        fake_device_sync_client_.get(), std::move(mock_timer));
  }

  void SetHostWithStatus(
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    mojom::HostStatus host_status =
        (host_device == std::nullopt ? mojom::HostStatus::kNoEligibleHosts
                                     : mojom::HostStatus::kHostVerified);
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
  }

  void SetIsFeatureEnabled(bool enabled) {
    delegate_->SetIsFeatureEnabled(enabled);

    HostStatusProvider::HostStatusWithDevice host_with_status =
        fake_host_status_provider_->GetHostWithStatus();
    if (host_with_status.host_status() != mojom::HostStatus::kHostVerified) {
      return;
    }

    multidevice::RemoteDeviceRef host_device = *host_with_status.host_device();

    bool enabled_on_backend =
        (host_device.GetSoftwareFeatureState(kTestHostFeature) ==
         multidevice::SoftwareFeatureState::kEnabled);
    bool pending_request_state_same_as_backend =
        (enabled == enabled_on_backend);

    if (pending_request_state_same_as_backend) {
      return;
    }

    VerifyLatestSetHostNetworkRequest(host_device, enabled);
  }

  void VerifyLatestSetHostNetworkRequest(
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
      EXPECT_EQ(kTestHostFeature, inputs.software_feature);
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
    EXPECT_EQ(kTestHostFeature, inputs.feature);
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

  void InvokePendingSetHostNetworkRequestCallback(
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

  void SetHostInDeviceSyncClient(
      const std::optional<multidevice::RemoteDeviceRef>& host_device,
      bool enabled) {
    GetMutableRemoteDevice(*host_device)->software_features[kTestHostFeature] =
        (enabled ? multidevice::SoftwareFeatureState::kEnabled
                 : multidevice::SoftwareFeatureState::kSupported);
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetFeatureFlags(bool enable_feature_flag) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (enable_feature_flag) {
      enabled_features.push_back(kTestFeatureFlag);
    } else {
      disabled_features.push_back(kTestFeatureFlag);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  FakeHostStatusProvider* fake_host_status_provider() {
    return fake_host_status_provider_.get();
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() {
    return fake_device_sync_client_.get();
  }

  base::MockOneShotTimer* mock_timer() { return mock_timer_; }

  GlobalStateFeatureManager* delegate() { return delegate_.get(); }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

  const multidevice::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;

  std::unique_ptr<GlobalStateFeatureManager> delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest, Success) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable the feature on host device and succeed
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);

  // Attempt to disable the feature on host device and succeed
  SetIsFeatureEnabled(false);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  SetHostInDeviceSyncClient(test_devices()[0], false /* enabled */);
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       NewDevicesSyncedBeforeCallback) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable the feature on host device and succeed
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
  // Triggers OnNewDevicesSynced
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  // Triggers Success Callback
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest, Failure) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable the feature on host device and fail
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  // A retry should have been scheduled, so fire the timer to start the retry.
  mock_timer()->Fire();

  // Simulate another failure.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       MultipleRequests_FirstFail_ThenSucceed) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable the feature on host device and fail
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  // The retry timer is running; however, instead of relying on that, call
  // SetIsFeatureEnabled() again to trigger an immediate
  // retry without the timer.
  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       PendingRequest_NoSyncedHostDevice) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable the feature on test_device 0
  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  // Fail to set host enabled on test_device 0
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
  EXPECT_TRUE(mock_timer()->IsRunning());

  // Remove synced device. This should remove the pending request and stop the
  // retry timer.
  SetHostInDeviceSyncClient(std::nullopt);
  SetHostWithStatus(std::nullopt);
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       InitialPendingEnableRequest_NoInitialDevice) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(std::nullopt /* initial_host */,
                 kPendingEnable /* initial_pending_state*/);

  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       InitialPendingEnableRequest_Success) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */,
                 kPendingEnable /* initial_pending_state*/);

  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       MultiplePendingRequests_EnableDisable) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable->disable the feature without invoking any
  // callbacks.
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  // The feature is already disabled on back-end so there should be no new
  // pending request
  SetIsFeatureEnabled(false);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       PendingRequest_SyncedHostBecomesUnverified) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */,
                 kPendingEnable /* initial_pending_state */);

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);

  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       Retrying_SyncedHostBecomesUnverified) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(mock_timer()->IsRunning());

  // Host becomes unverified, this should stop timer and clear pending request
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);
  EXPECT_FALSE(mock_timer()->IsRunning());
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       FailureCallback_SyncedHostBecomesUnverified) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  // Set host unverified. This should reset pending request.
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);

  // Invoke failure callback. No retry should be scheduled.
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(mock_timer()->IsRunning());
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       NoVerifiedHost_AttemptToEnable) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);

  // Attempt to enable the feature on host device
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  SetIsFeatureEnabled(true);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       StatusChangedOnRemoteDevice) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);

  // Simulate enabled on a remote device.
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SimultaneousRequests_StartOff_ToggleOnOff) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to enable
  SetIsFeatureEnabled(true);
  // Attempt to disable
  SetIsFeatureEnabled(false);

  // Only one network request should be in flight at a time
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());

  // Successfully enable on host
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);

  // A new network request should be scheduled to disable
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SetHostInDeviceSyncClient(test_devices()[0], false /* enabled */);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SetPendingEnableOnVerify_HostSetLocallyThenHostVerified) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(std::nullopt /* initial_host */);

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  SetHostInDeviceSyncClient(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kSetPendingEnableOnVerify);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingEnable);
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_F(
    MultiDeviceSetupGlobalStateFeatureManagerImplTest,
    SetPendingEnableOnVerify_HostSetLocallyThenHostSetNotVerifiedThenHostVerified) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(std::nullopt /* initial_host */);

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  SetHostInDeviceSyncClient(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kSetPendingEnableOnVerify);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kSetPendingEnableOnVerify);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());

  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingEnable);
  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SetPendingEnableOnVerify_FeatureFlagOff) {
  SetFeatureFlags(false /* enable_feature_flag */);
  CreateDelegate(std::nullopt /* initial_host */);

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  SetHostInDeviceSyncClient(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SetPendingEnableOnVerify_FeatureNotAllowedByPolicy) {
  SetFeatureFlags(true /* enable_feature_flag */);
  // Disable by policy
  test_pref_service()->SetBoolean(kFeatureAllowedPrefName, false);
  CreateDelegate(std::nullopt /* initial_host */);

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  SetHostInDeviceSyncClient(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SetPendingEnableOnVerify_FeatureNotSupportedOnHostDevice) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(std::nullopt /* initial_host */);
  GetMutableRemoteDevice(test_devices()[0])
      ->software_features[kTestHostFeature] =
      multidevice::SoftwareFeatureState::kNotSupported;

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  SetHostInDeviceSyncClient(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SetPendingEnableOnVerify_HostRemoved) {
  SetFeatureFlags(true /* enable_feature_flag */);
  CreateDelegate(std::nullopt /* initial_host */);

  // kHostSetLocallyButWaitingForBackendConfirmation is only possible if the
  // setup flow has been completed on the local device.
  SetHostInDeviceSyncClient(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kSetPendingEnableOnVerify);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());

  // Host is added but not verified.
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kSetPendingEnableOnVerify);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());

  // Host is removed before it was verified. This simulates the user going
  // through the forget phone flow before the phone was able to be verified.
  // The feature manager should stop the enable attempt because it requires a
  // paired host device that transitions from unverified to verified.
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet, std::nullopt);
  EXPECT_EQ(test_pref_service()->GetInteger(kPendingStatePrefName),
            kPendingNone);
  EXPECT_FALSE(delegate()->IsFeatureEnabled());
}

TEST_F(MultiDeviceSetupGlobalStateFeatureManagerImplTest,
       SetPendingEnableOnVerify_InitialPendingRequest) {
  SetFeatureFlags(true /* enable_feature_flag */);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  CreateDelegate(test_devices()[0] /* initial_host */,
                 kSetPendingEnableOnVerify /* initial_pending_state */);

  EXPECT_TRUE(delegate()->IsFeatureEnabled());
  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kSupported);
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_EQ(0, GetSetHostNetworkRequestCallbackQueueSize());
  SetHostInDeviceSyncClient(test_devices()[0], true /* enabled */);

  EXPECT_EQ(test_devices()[0].GetSoftwareFeatureState(kTestHostFeature),
            multidevice::SoftwareFeatureState::kEnabled);
}

}  // namespace multidevice_setup

}  // namespace ash
