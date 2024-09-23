// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/secure_channel_tether_availability_operation_orchestrator.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_connection_preserver.h"
#include "chromeos/ash/components/tether/fake_tether_availability_operation.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
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

  void SetUp() override {}

  // TetherAvailabilityOperationOrchestrator::Observer:
  void OnTetherAvailabilityResponse(
      const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
      const std::vector<ScannedDeviceInfo>&
          gms_core_notifications_disabled_devices,
      bool is_final_scan_result) override {
    scanned_device_list_so_far_ = scanned_device_list_so_far;
    gms_core_notifications_disabled_devices_ =
        gms_core_notifications_disabled_devices;
    is_final_scan_ = is_final_scan_result;
  }

  std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
  BuildOrchestrator(FakeTetherHostFetcher* fake_tether_host_fetcher,
                    FakeTetherAvailabilityOperation::Initializer*
                        fake_tether_availability_operation_initializer) {
    std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
        orchestrator = base::WrapUnique(
            new SecureChannelTetherAvailabilityOperationOrchestrator(
                base::WrapUnique<TetherAvailabilityOperation::Initializer>(
                    fake_tether_availability_operation_initializer),
                fake_tether_host_fetcher));

    orchestrator->AddObserver(this);
    return orchestrator;
  }

  ScannedDeviceInfo CreateFakeScannedDeviceInfo(const TetherHost& tether_host) {
    auto device_status = CreateTestDeviceStatus(
        "Google Fi", 75 /* battery_percentage */, 4 /* connection_strength */);
    return ScannedDeviceInfo(
        tether_host.GetDeviceId(), tether_host.GetName(), device_status,
        /*setup_required=*/false, /*notifications_enabled=*/true);
  }

  std::vector<ScannedDeviceInfo> scanned_device_list_so_far_;
  std::vector<ScannedDeviceInfo> gms_core_notifications_disabled_devices_;
  bool is_final_scan_;

 protected:
  SecureChannelTetherAvailabilityOperationOrchestratorTest() {}

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SecureChannelTetherAvailabilityOperationOrchestratorTest,
       HostFetcher_StartsOperation) {
  const TetherHost test_device =
      TetherHost(multidevice::CreateRemoteDeviceRefForTest());
  FakeTetherHostFetcher fake_tether_host_fetcher(
      *test_device.remote_device_ref());
  fake_tether_host_fetcher.SetTetherHost(
      test_device.remote_device_ref().value());
  FakeTetherAvailabilityOperation::Initializer*
      fake_tether_availability_operation_initializer =
          new FakeTetherAvailabilityOperation::Initializer();
  std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
      orchestrator =
          BuildOrchestrator(&fake_tether_host_fetcher,
                            fake_tether_availability_operation_initializer);
  orchestrator->Start();

  // Expect an operation was started for each of the expected devices.
  EXPECT_TRUE(fake_tether_availability_operation_initializer
                  ->has_active_operation_for_device(test_device));

  auto scanned_device_info = CreateFakeScannedDeviceInfo(test_device);
  fake_tether_availability_operation_initializer->send_result(
      test_device, scanned_device_info);
  EXPECT_EQ(1u, scanned_device_list_so_far_.size());
  EXPECT_TRUE(base::Contains(scanned_device_list_so_far_, scanned_device_info));
}

TEST_F(SecureChannelTetherAvailabilityOperationOrchestratorTest,
       HostFetcher_EndsOrchestratorIfNoDevices) {
  FakeTetherHostFetcher fake_tether_host_fetcher(/*tether_host=*/std::nullopt);
  FakeTetherAvailabilityOperation::Initializer*
      fake_tether_availability_operation_initializer =
          new FakeTetherAvailabilityOperation::Initializer();
  std::unique_ptr<SecureChannelTetherAvailabilityOperationOrchestrator>
      orchestrator =
          BuildOrchestrator(&fake_tether_host_fetcher,
                            fake_tether_availability_operation_initializer);
  orchestrator->Start();
  EXPECT_EQ(0u, scanned_device_list_so_far_.size());
  EXPECT_EQ(0u, gms_core_notifications_disabled_devices_.size());
  EXPECT_TRUE(is_final_scan_);
}

}  // namespace ash::tether
