// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/mock_timer.h"
#include "base/unguessable_token.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const char kPendingRequestHostIdPrefName[] =
    "multidevice_setup.pending_request_host_id";
const char kPendingRemovalOfCurrentHost[] = "pendingRemovalOfCurrentHost";
const char kNoPendingRequest[] = "";

const size_t kNumTestDevices = 5;

}  // namespace

class MultiDeviceSetupHostBackendDelegateImplTest : public testing::Test {
 protected:
  MultiDeviceSetupHostBackendDelegateImplTest()
      : test_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupHostBackendDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
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

  void TearDown() override { delegate_->RemoveObserver(observer_.get()); }

  void CreateDelegate(
      const base::Optional<cryptauth::RemoteDeviceRef>& initial_host,
      const std::string& initial_pending_host_request = kNoPendingRequest) {
    SetHostInDeviceSyncClient(initial_host);
    test_pref_service_->SetString(kPendingRequestHostIdPrefName,
                                  initial_pending_host_request);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    delegate_ = HostBackendDelegateImpl::Factory::Get()->BuildInstance(
        fake_eligible_host_devices_provider_.get(), test_pref_service_.get(),
        fake_device_sync_client_.get(), std::move(mock_timer));
    EXPECT_EQ(initial_host, delegate_->GetMultiDeviceHostFromBackend());

    observer_ = std::make_unique<FakeHostBackendDelegateObserver>();
    delegate_->AddObserver(observer_.get());
  }

  int GetSetSoftwareFeatureStateCallbackQueueSize() {
    return fake_device_sync_client_
        ->GetSetSoftwareFeatureStateCallbackQueueSize();
  }

  void InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult result_code,
      bool expected_to_notify_observer_and_start_retry_timer) {
    size_t num_failure_events_before_call =
        observer_->num_failed_backend_requests();

    fake_device_sync_client_->InvokePendingSetSoftwareFeatureStateCallback(
        result_code);

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
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device_after_sync,
      bool expected_to_fulfill_pending_request) {
    base::Optional<cryptauth::RemoteDeviceRef> host_device_before_call =
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
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
    bool attempting_to_set_host_which_already_exists =
        host_device == delegate_->GetMultiDeviceHostFromBackend();
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

    // TODO(khorimoto): Check that the parameters passed to
    // |fake_device_sync_client_| are correct. Currently, FakeDeviceSyncClient
    // does provide a mechanism for checking these parameters.
  }

  void SetHostInDeviceSyncClient(
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
    for (const auto& remote_device : test_devices_) {
      bool should_be_host =
          host_device != base::nullopt &&
          host_device->GetDeviceId() == remote_device.GetDeviceId();

      GetMutableRemoteDevice(remote_device)
          ->software_features
              [cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST] =
          should_be_host ? cryptauth::SoftwareFeatureState::kEnabled
                         : cryptauth::SoftwareFeatureState::kSupported;
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

  const cryptauth::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

 private:
  cryptauth::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeEligibleHostDevicesProvider>
      fake_eligible_host_devices_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  base::MockOneShotTimer* mock_timer_;

  std::unique_ptr<FakeHostBackendDelegateObserver> observer_;

  std::unique_ptr<HostBackendDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupHostBackendDelegateImplTest);
};

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, Success) {
  CreateDelegate(base::nullopt /* initial_host */);

  // Set device 0.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Remove device 0 such that there is no longer a host..
  AttemptToSetMultiDeviceHostOnBackend(base::nullopt);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetPendingHostRequest());
  SimulateNewHostDevicesSynced(base::nullopt /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Set device 1.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[1]);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
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
  CreateDelegate(base::nullopt /* initial_host */);

  // Attempt to set device 0, but fail.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // A retry should have been scheduled, so fire the timer to start the retry.
  mock_timer()->Fire();

  // Simulate another failure.
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 1, but fail.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[1]);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[1], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());
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

  // Fire the callback for device 1, but have it fail. This is not expected to
  // notify the observer or start the retry timer, since the failure was for
  // device 1's request and device 3 is the pending host request.
  EXPECT_EQ(3, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the callback for device 2, and have it succeed. This should affect the
  // value of GetMultiDeviceHostFromBackend(), but there should still be a
  // pending request for device 3.
  EXPECT_EQ(2, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[2] /* host_device_after_sync */,
                               false /* expected_to_fulfill_pending_request */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetPendingHostRequest());
  EXPECT_EQ(test_devices()[2], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the callback for device 3, and have it succeed.
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[3] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[3], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       SimultaneousRequestsToSameDevice) {
  CreateDelegate(base::nullopt /* initial_host */);

  // Attempt to set device 0, but do not invoke the callback yet.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 0 again, and still do not invoke the callback.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Attempt to set device 0 one more time.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // Fire the first callback, which should successfully transition the host.
  EXPECT_EQ(3, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(test_devices()[0] /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the second callback, but have it fail. No state should be affected.
  EXPECT_EQ(2, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());

  // Fire the third callback, and have it succeed. Still, no state should be
  // affected.
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       MultipleRequestsToSameDevice_FirstFail_ThenSucceed) {
  CreateDelegate(base::nullopt /* initial_host */);

  // Attempt to set device 0, but fail.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kOffline,
      true /* expected_to_notify_observer_and_start_retry_timer */);
  EXPECT_TRUE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0], delegate()->GetPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());

  // The retry timer is running; however, instead of relying on that, call
  // AttemptToSetMultiDeviceHostOnBackend() again to trigger an immediate retry
  // without the timer.
  AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
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
      base::nullopt /* initial_host */,
      test_devices()[0].GetDeviceId() /* initial_pending_host_request */);

  // The delegate should have started a request as soon as it was created.
  // Simulate it succeeding.
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
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
  EXPECT_EQ(1, GetSetSoftwareFeatureStateCallbackQueueSize());
  InvokePendingSetSoftwareFeatureStateCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      false /* expected_to_notify_observer_and_start_retry_timer */);
  SimulateNewHostDevicesSynced(base::nullopt /* host_device_after_sync */,
                               true /* expected_to_fulfill_pending_request */);
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
  EXPECT_EQ(base::nullopt, delegate()->GetMultiDeviceHostFromBackend());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, ChangedFromOtherDevice) {
  CreateDelegate(base::nullopt /* initial_host */);

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
  CreateDelegate(base::nullopt /* initial_host */,
                 "nonexistentDeviceId" /* initial_pending_host_request */);

  // An initial pending host request exists, but it is for a host that is not
  // present in the DeviceSyncClient. Thus, the request should be canceled.
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest,
       PendingRequestCanceledIfDeviceToRemoveNoLongerExists) {
  CreateDelegate(
      base::nullopt /* initial_host */,
      kPendingRemovalOfCurrentHost /* initial_pending_host_request */);

  // An initial pending host request exists to remove the current host, but
  // there actually is no current host. Thus, the request should be canceled.
  EXPECT_FALSE(delegate()->HasPendingHostRequest());
}

TEST_F(MultiDeviceSetupHostBackendDelegateImplTest, TryToSetNonEligibleHost) {
  // Make all test devices ineligible.
  fake_eligible_host_devices_provider()->set_eligible_host_devices(
      cryptauth::RemoteDeviceRefList());

  CreateDelegate(base::nullopt /* initial_host */);

  delegate()->AttemptToSetMultiDeviceHostOnBackend(test_devices()[0]);
  EXPECT_EQ(0u, observer()->num_pending_host_request_changes());
}

}  // namespace multidevice_setup

}  // namespace chromeos
