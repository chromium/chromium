// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/secure_channel_tether_availability_operation_orchestrator.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

class SecureChannelTetherAvailabilityOperationOrchestratorTest
    : public testing::Test,
      public TetherAvailabilityOperationOrchestrator::Observer {
 public:
  SecureChannelTetherAvailabilityOperationOrchestratorTest(
      const SecureChannelTetherAvailabilityOperationOrchestratorTest&) = delete;
  SecureChannelTetherAvailabilityOperationOrchestratorTest& operator=(
      const SecureChannelTetherAvailabilityOperationOrchestratorTest&) = delete;

  // TetherAvailabilityOperationOrchestrator::Observer:
  void OnTetherAvailabilityResponse(
      const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
      const multidevice::RemoteDeviceRefList&
          gms_core_notifications_disabled_devices,
      bool is_final_scan_result) override {
    scanned_device_list_so_far_ = scanned_device_list_so_far;
    gms_core_notifications_disabled_devices_ =
        gms_core_notifications_disabled_devices;
    is_final_scan_ = is_final_scan_result;
  }

  std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
  BuildOrchestrator(FakeTetherHostFetcher* fake_tether_host_fetcher) {
    std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
        orchestrator = std::make_unique<
            SecureChannelTetherAvailabilityOperationOrchestrator>(
            fake_tether_host_fetcher);

    orchestrator->AddObserver(this);
    return orchestrator;
  }

  std::vector<ScannedDeviceInfo> scanned_device_list_so_far_;
  multidevice::RemoteDeviceRefList gms_core_notifications_disabled_devices_;
  bool is_final_scan_;

 protected:
  SecureChannelTetherAvailabilityOperationOrchestratorTest() {}
};

TEST_F(SecureChannelTetherAvailabilityOperationOrchestratorTest,
       HostFetcher_WillFetchAllDevices) {
  const multidevice::RemoteDeviceRefList& expected_devices =
      multidevice::CreateRemoteDeviceRefListForTest(4);
  FakeTetherHostFetcher fake_tether_host_fetcher(expected_devices);
  std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
      orchestrator = BuildOrchestrator(&fake_tether_host_fetcher);
  orchestrator->Start();
  EXPECT_EQ(expected_devices, orchestrator->fetched_tether_hosts_);
}

TEST_F(SecureChannelTetherAvailabilityOperationOrchestratorTest,
       HostFetcher_EndsOrchestratorIfNoDevices) {
  const multidevice::RemoteDeviceRefList& expected_devices =
      multidevice::CreateRemoteDeviceRefListForTest(0);
  FakeTetherHostFetcher fake_tether_host_fetcher(expected_devices);
  std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
      orchestrator = BuildOrchestrator(&fake_tether_host_fetcher);
  orchestrator->Start();
  EXPECT_EQ(0u, scanned_device_list_so_far_.size());
  EXPECT_EQ(0u, gms_core_notifications_disabled_devices_.size());
  EXPECT_TRUE(is_final_scan_);
}

}  // namespace ash::tether
