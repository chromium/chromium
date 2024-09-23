// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_status_provider_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_verifier.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 5;

}  // namespace

class MultiDeviceSetupHostStatusProviderImplTest : public testing::Test {
 public:
  MultiDeviceSetupHostStatusProviderImplTest(
      const MultiDeviceSetupHostStatusProviderImplTest&) = delete;
  MultiDeviceSetupHostStatusProviderImplTest& operator=(
      const MultiDeviceSetupHostStatusProviderImplTest&) = delete;

 protected:
  MultiDeviceSetupHostStatusProviderImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupHostStatusProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_eligible_host_devices_provider_ =
        std::make_unique<FakeEligibleHostDevicesProvider>();
    fake_host_backend_delegate_ = std::make_unique<FakeHostBackendDelegate>();
    fake_host_verifier_ = std::make_unique<FakeHostVerifier>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);

    host_status_provider_ = HostStatusProviderImpl::Factory::Create(
        fake_eligible_host_devices_provider_.get(),
        fake_host_backend_delegate_.get(), fake_host_verifier_.get(),
        fake_device_sync_client_.get());

    fake_observer_ = std::make_unique<FakeHostStatusProviderObserver>();
    host_status_provider_->AddObserver(fake_observer_.get());
  }

  void TearDown() override {
    host_status_provider_->RemoveObserver(fake_observer_.get());
  }

  void MakeDevicesEligibleHosts() {
    fake_eligible_host_devices_provider_->set_eligible_host_devices(
        test_devices_);
    fake_eligible_host_devices_provider_
        ->NotifyObserversEligibleDevicesSynced();
  }

  // Verifies the current status and, if |expected_observer_index| is non-null,
  // verifies that the observer received that update at the specified index.
  void VerifyCurrentStatus(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device,
      const std::optional<size_t>& expected_observer_index) {
    HostStatusProvider::HostStatusWithDevice status_with_device(host_status,
                                                                host_device);
    EXPECT_EQ(status_with_device, host_status_provider_->GetHostWithStatus());

    if (!expected_observer_index)
      return;
    EXPECT_EQ(status_with_device,
              fake_observer_->host_status_updates()[*expected_observer_index]);
  }

  size_t GetNumChangeEvents() {
    return fake_observer_->host_status_updates().size();
  }

  const multidevice::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

  FakeHostBackendDelegate* fake_host_backend_delegate() {
    return fake_host_backend_delegate_.get();
  }

  FakeHostVerifier* fake_host_verifier() { return fake_host_verifier_.get(); }

  FakeEligibleHostDevicesProvider* fake_eligible_host_devices_provider() {
    return fake_eligible_host_devices_provider_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeEligibleHostDevicesProvider>
      fake_eligible_host_devices_provider_;
  std::unique_ptr<FakeHostBackendDelegate> fake_host_backend_delegate_;
  std::unique_ptr<FakeHostVerifier> fake_host_verifier_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeHostStatusProviderObserver> fake_observer_;

  std::unique_ptr<HostStatusProvider> host_status_provider_;
};

TEST_F(MultiDeviceSetupHostStatusProviderImplTest,
       IncreaseHostState_ThenDecrease) {
  VerifyCurrentStatus(mojom::HostStatus::kNoEligibleHosts,
                      std::nullopt /* host_device */,
                      std::nullopt /* expected_observer_index */);

  // Add eligible hosts to the account and verify that the status has been
  // updated accordingly.
  MakeDevicesEligibleHosts();
  EXPECT_EQ(1u, GetNumChangeEvents());
  VerifyCurrentStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                      std::nullopt /* host_device */,
                      0u /* expected_observer_index */);

  // Make a request to set the host, but do not complete it yet.
  fake_host_backend_delegate()->AttemptToSetMultiDeviceHostOnBackend(
      test_devices()[0]);
  EXPECT_EQ(2u, GetNumChangeEvents());
  VerifyCurrentStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0] /* host_device */, 1u /* expected_observer_index */);

  // Successfully set the host on the back-end.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(test_devices()[0]);
  EXPECT_EQ(3u, GetNumChangeEvents());
  VerifyCurrentStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                      test_devices()[0] /* host_device */,
                      2u /* expected_observer_index */);

  // Verify the device.
  fake_host_verifier()->set_is_host_verified(true);
  fake_host_verifier()->NotifyHostVerified();
  EXPECT_EQ(4u, GetNumChangeEvents());
  VerifyCurrentStatus(mojom::HostStatus::kHostVerified,
                      test_devices()[0] /* host_device */,
                      3u /* expected_observer_index */);
}

TEST_F(MultiDeviceSetupHostStatusProviderImplTest,
       OnNewDevicesSyncedNotifiesHostStatusChange) {
  MakeDevicesEligibleHosts();
  EXPECT_EQ(1u, GetNumChangeEvents());
  fake_eligible_host_devices_provider()->NotifyObserversEligibleDevicesSynced();
  EXPECT_EQ(2u, GetNumChangeEvents());
}

TEST_F(MultiDeviceSetupHostStatusProviderImplTest, SetHostThenForget) {
  MakeDevicesEligibleHosts();
  EXPECT_EQ(1u, GetNumChangeEvents());
  VerifyCurrentStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                      std::nullopt /* host_device */,
                      0u /* expected_observer_index */);

  // Without first attempting to set the host on the back-end, set the device.
  // This simulates the case where the host is set by another device (e.g., if
  // another Chromebook completes the setup flow).
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(test_devices()[0]);
  EXPECT_EQ(2u, GetNumChangeEvents());
  VerifyCurrentStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                      test_devices()[0] /* host_device */,
                      1u /* expected_observer_index */);

  // Now, start an attempt to remove the device on the back-end. This simulates
  // the user clicking "forget device" in settings.
  fake_host_backend_delegate()->AttemptToSetMultiDeviceHostOnBackend(
      std::nullopt /* host_device */);
  EXPECT_EQ(3u, GetNumChangeEvents());
  VerifyCurrentStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                      std::nullopt /* host_device */,
                      2u /* expected_observer_index */);

  // Complete the pending back-end request. In this case, the status should stay
  // the same, so the observer should not have received an additional event.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(
      std::nullopt /* host_device_on_backend */);
  EXPECT_EQ(3u, GetNumChangeEvents());
}

}  // namespace multidevice_setup

}  // namespace ash
