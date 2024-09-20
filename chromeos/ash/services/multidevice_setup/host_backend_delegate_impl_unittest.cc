// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_backend_delegate_impl.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_backend_delegate.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const char kPendingRequestHostIdPrefName[] =
    "multidevice_setup.pending_request_host_id";
const char kPendingRemovalOfCurrentHost[] = "pendingRemovalOfCurrentHost";
const char kNoPendingRequest[] = "";

const size_t kNumTestDevices = 4;

}  // namespace

class MultiDeviceSetupHostBackendDelegateImplTest
    : public ::testing::TestWithParam<bool> {
 public:
  MultiDeviceSetupHostBackendDelegateImplTest(
      const MultiDeviceSetupHostBackendDelegateImplTest&) = delete;
  MultiDeviceSetupHostBackendDelegateImplTest& operator=(
      const MultiDeviceSetupHostBackendDelegateImplTest&) = delete;

 protected:
  MultiDeviceSetupHostBackendDelegateImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupHostBackendDelegateImplTest() override = default;

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

    fake_eligible_host_devices_provider_ =
        std::make_unique<FakeEligibleHostDevicesProvider>();
    fake_eligible_host_devices_provider_->set_eligible_host_devices(
        test_devices_);

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    HostBackendDelegateImpl::RegisterPrefs(test_pref_service_->registry());

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);
  }

  void TearDown() override {
    if (delegate_)
      delegate_->RemoveObserver(observer_.get());
  }

  void CreateDelegate(
      const std::optional<multidevice::RemoteDeviceRef>& initial_host,
      const std::string& initial_pending_host_request = kNoPendingRequest) {
    SetHostInDeviceSyncClient(initial_host);
    test_pref_service_->SetString(kPendingRequestHostIdPrefName,
                                  initial_pending_host_request);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    delegate_ = HostBackendDelegateImpl::Factory::Create(
        fake_eligible_host_devices_provider_.get(), test_pref_service_.get(),
        fake_device_sync_client_.get(), std::move(mock_timer));
    EXPECT_EQ(initial_host, delegate_->GetMultiDeviceHostFromBackend());

    observer_ = std::make_unique<FakeHostBackendDelegateObserver>();
    delegate_->AddObserver(observer_.get());
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
    size_t num_failure_events_before_call =
        observer_->num_failed_backend_requests();

    if (features::ShouldUseV1DeviceSync()) {
      fake_device_sync_client_->InvokePendingSetSoftwareFeatureStateCallback(
          result_code);
    } else {
      fake_device_sync_client_->InvokePendingSetFeatureStatusCallback(
          result_code);
    }

    if (expected_to_notify_observer_and_start_retry_timer) {
      EXPECT_EQ(num_failure_events_before_call + 1u,
                observer_->num_failed_backend_requests());
    } else {
      EXPECT_EQ(num_failure_events_before_call,
                observer_->num_failed_backend_requests());
    }

    EXPECT_EQ(expected_to_notify_observer_and_start_retry_timer,
              mock_timer_->IsRunning());
  }

  void SimulateNewHostDevicesSynced(
      const std::optional<multidevice::RemoteDeviceRef>& host_device_after_sync,
      bool expected_to_fulfill_pending_request) {
    std::optional<multidevice::RemoteDeviceRef> host_device_before_call =
        delegate_->GetMultiDeviceHostFromBackend();
    bool host_changed = host_device_before_call != host_device_after_sync;
    size_t num_host_change_events_before_call =
        observer_->num_changes_on_backend();
    size_t num_pending_host_request_change_events_before_call =
        observer_->num_pending_host_request_changes();

    SetHostInDeviceSyncClient(host_device_after_sync);
    fake_device_sync_client_->NotifyNewDevicesSynced();

    if (host_changed) {
      EXPECT_EQ(num_host_change_events_before_call + 1u,
                observer_->num_changes_on_backend());
    } else {
      EXPECT_EQ(num_host_change_events_before_call,
                observer_->num_changes_on_backend());
    }

    if (expected_to_fulfill_pending_request) {
      EXPECT_FALSE(delegate_->HasPendingHostRequest());

      // Expected to change from a pending request to no request.
      EXPECT_EQ(num_pending_host_request_change_events_before_call + 1u,
                observer_->num_pending_host_request_changes());
    } else {
      EXPECT_EQ(num_pending_host_request_change_events_before_call,
                observer_->num_pending_host_request_changes());
    }
  }

  void AttemptToSetMultiDeviceHostOnBackend(
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    std::optional<multidevice::RemoteDeviceRef> host_before_call =
        delegate_->GetMultiDeviceHostFromBackend();
    bool attempting_to_set_host_which_already_exists =
        host_device == host_before_call;
    size_t num_pending_host_request_change_events_before_call =
        observer_->num_pending_host_request_changes();
    bool was_request_for_same_device_as_pending_request =
        delegate_->HasPendingHostRequest() &&
        delegate_->GetPendingHostRequest() == host_device;

    delegate_->AttemptToSetMultiDeviceHostOnBackend(host_device);

    // A new attempt means that any previous retry attempts should have been
    // canceled.
    EXPECT_FALSE(mock_timer_->IsRunning());

    if (attempting_to_set_host_which_already_exists) {
      EXPECT_FALSE(delegate_->HasPendingHostRequest());
      return;
    }

    EXPECT_EQ(host_device, delegate_->GetPendingHostRequest());

    if (was_request_for_same_device_as_pending_request) {
      EXPECT_EQ(num_pending_host_request_change_events_before_call,
                observer_->num_pending_host_request_changes());
    } else {
      EXPECT_EQ(num_pending_host_request_change_events_before_call + 1u,
                observer_->num_pending_host_request_changes());
    }

    // Verify that the correct parameters were passed to
    // SetSoftwareFeatureState() or SetFeatureStatus().
    if (host_device) {
      VerifyLatestSetHostNetworkRequest(*host_device, true /* should_enable */);
    } else {
      ASSERT_TRUE(host_before_call);
      VerifyLatestSetHostNetworkRequest(*host_before_call,
                                        false /* should_enable */);
    }
  }

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
  }

  FakeEligibleHostDevicesProvider* fake_eligible_host_devices_provider() {
    return fake_eligible_host_devices_provider_.get();
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() {
    return fake_device_sync_client_.get();
  }

  FakeHostBackendDelegateObserver* observer() { return observer_.get(); }

  base::MockOneShotTimer* mock_timer() { return mock_timer_; }

  HostBackendDelegate* delegate() { return delegate_.get(); }

  const multidevice::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

 private:
  void VerifyLatestSetHostNetworkRequest(
      const multidevice::RemoteDeviceRef expected_host,
      bool expected_should_enable) {
    // Verify inputs to SetSoftwareFeatureState().
    if (features::ShouldUseV1DeviceSync()) {
      ASSERT_FALSE(
          fake_device_sync_client_->set_software_feature_state_inputs_queue()
              .empty());
      const device_sync::FakeDeviceSyncClient::SetSoftwareFeatureStateInputs&
          inputs = fake_device_sync_client_
                       ->set_software_feature_state_inputs_queue()
                       .back();
      EXPECT_EQ(expected_host.public_key(), inputs.public_key);
      EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
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
    EXPECT_EQ(multidevice::SoftwareFeature::kBetterTogetherHost,
              inputs.feature);
    EXPECT_EQ(expected_should_enable
                  ? device_sync::FeatureStatusChange::kEnableExclusively
                  : device_sync::FeatureStatusChange::kDisable,
              inputs.status_change);
  }

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeEligibleHostDevicesProvider>
      fake_eligible_host_devices_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;

  std::unique_ptr<FakeHostBackendDelegateObserver> observer_;

  std::unique_ptr<HostBackendDelegate> delegate_;
};

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, Success) {
  CreateDelegate(std::nullopt /* initial_host */);

  // Set device 0.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Remove device 0 such that there is no longer a host.
  AttemptToSetMultiDeviceHostOnBackend(std::nullopt);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetPendingHostRequest());
  SimulateNewHostDevicesSynced(std::nullopt /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Set device 1.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[1]);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[1], delegate()->GetPendingHostRequest());
  SimulateNewHostDevicesSynced(test_devices()[1] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[1], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, Failure) {
  CreateDelegate(std::nullopt /* initial_host */);

  // Attempt to set device 0, but fail.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // A retry should have been scheduled, so fire the timer to start the retry.
  mock_timer()->Fire();

  // Simulate another failure.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 1, but fail.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[1]);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[1], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       StartWithDevice_SimultaneousRequests) {
  // Start with device 0 as the active host.
  CreateDelegate(test_devices()[0] /* initial_host */);

  // Attempt to set device 1, but do not invoke the callback yet.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[1]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[1], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 2, but do not invoke device 1's callback yet.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[2]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[2], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 3.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[3]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Note: Below, we assume that the feature setting requests are processed in
  // the order they are called. This is an assumption made in the
  // HostBackendDelegate implementation.

  // Fire the callback for device 1, but have it fail. This is not expected to
  // notify the observer or start the retry timer, since the failure was for
  // device 1's request and device 3 is the pending host request.
  EXPECT_EQ(3, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the callback for device 2, and have it succeed. This should affect the
  // value of GetMultiDeviceHostFromBackend(), but there should still be a
  // pending request for device 3.
  EXPECT_EQ(2, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[2] /* host_device_after_sync */,
                               false /* expected_to_fulfill_pending_request */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[2], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the callback for device 3, and have it succeed.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[3] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       SimultaneousRequestsToSameDevice) {
  CreateDelegate(std::nullopt /* initial_host */);

  // Attempt to set device 0, but do not invoke the callback yet.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 0 again, and still do not invoke the callback.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 0 one more time.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Fire the first callback, which should successfully transition the host.
  EXPECT_EQ(3, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the second callback, but have it fail. No state should be affected.
  EXPECT_EQ(2, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the third callback, and have it succeed. Still, no state should be
  // affected.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       MultipleRequestsToSameDevice_FirstFail_ThenSucceed) {
  CreateDelegate(std::nullopt /* initial_host */);

  // Attempt to set device 0, but fail.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // The retry timer is running; however, instead of relying on that, call
  // AttemptToSetMultiDeviceHostOnBackend() again to trigger an immediate retry
  // without the timer.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       InitialPendingRequestButNoInitialDevice) {
  CreateDelegate(
      std::nullopt /* initial_host */,
      features::ShouldUseV1DeviceSync()
          ? test_devices()[0].GetDeviceId()
          : test_devices()[0].instance_id() /* initial_pending_host_request */);

  // The delegate should have started a request as soon as it was created.
  // Simulate it succeeding.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       InitialDeviceWithPendingRequestToRemoveIt) {
  CreateDelegate(
      test_devices()[0] /* initial_host */,
      kPendingRemovalOfCurrentHost /* initial_pending_host_request */);

  // The delegate should have started a request as soon as it was created.
  // Simulate it succeeding.
  EXPECT_EQ(1, GetSetHostNetworkRequestCallbackQueueSize());
  InvokePendingSetHostNetworkRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(std::nullopt /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(std::nullopt, delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, ChangedFromOtherDevice) {
  CreateDelegate(std::nullopt /* initial_host */);

  // The device changed from another device (i.e.,
  // AttemptToSetMultiDeviceHostOnBackend() was not called).
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               false /* expected_to_fulfill_pending_request */);

  // One more change.
  SimulateNewHostDevicesSynced(test_devices()[1] /* host_device_after_sync */,
                               false /* expected_to_fulfill_pending_request */);
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       PendingRequestCanceledIfDeviceToSetNoLongerExists) {
  CreateDelegate(std::nullopt /* initial_host */,
                 "nonexistentDeviceId" /* initial_pending_host_request */);

  // An initial pending host request exists, but it is for a host that is not
  // present in the DeviceSyncClient. Thus, the request should be canceled.
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       PendingRequestCanceledIfDeviceToRemoveNoLongerExists) {
  CreateDelegate(
      std::nullopt /* initial_host */,
      kPendingRemovalOfCurrentHost /* initial_pending_host_request */);

  // An initial pending host request exists to remove the current host, but
  // there actually is no current host. Thus, the request should be canceled.
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, TryToSetNonEligibleHost) {
  // Make all test devices ineligible.
  fake_eligible_host_devices_provider()->set_eligible_host_devices(
      multidevice::RemoteDeviceRefList());

  CreateDelegate(std::nullopt /* initial_host */);

  delegate()->AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(0u, observer()->num_pending_host_request_changes());
}

}  // namespace multidevice_setup

}  // namespace ash
