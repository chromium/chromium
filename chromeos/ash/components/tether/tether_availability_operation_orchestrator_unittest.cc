// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_availability_operation_orchestrator.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_tether_availability_operation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

class FakeTetherAvailabilityOperationOrchestrator
    : public TetherAvailabilityOperationOrchestrator {
 public:
  explicit FakeTetherAvailabilityOperationOrchestrator(
      std::unique_ptr<TetherAvailabilityOperation::Initializer>
          tether_availability_operation_initializer)
      : TetherAvailabilityOperationOrchestrator(
            std::move(tether_availability_operation_initializer)) {}

  ~FakeTetherAvailabilityOperationOrchestrator() override {}

  void Start() override {}

  void start_operation_for_device(const TetherHost& tether_host) {
    StartOperation(tether_host);
  }
};

class TetherAvailabilityOperationOrchestratorTest
    : public testing::Test,
      public TetherAvailabilityOperationOrchestrator::Observer {
 public:
  TetherAvailabilityOperationOrchestratorTest(
      const TetherAvailabilityOperationOrchestratorTest&) = delete;
  TetherAvailabilityOperationOrchestratorTest& operator=(
      const TetherAvailabilityOperationOrchestratorTest&) = delete;

  void OnTetherAvailabilityResponse(
      const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
      const std::vector<ScannedDeviceInfo>&
          gms_core_notifications_disabled_devices,
      bool is_final_scan_result) override {
    scanned_device_list_so_far_ = scanned_device_list_so_far;
    gms_core_notifications_disabled_devices_ =
        gms_core_notifications_disabled_devices;
    is_final_scan_result_ = is_final_scan_result;
  }

 protected:
  TetherAvailabilityOperationOrchestratorTest() {}
  std::vector<ScannedDeviceInfo> scanned_device_list_so_far_;
  std::vector<ScannedDeviceInfo> gms_core_notifications_disabled_devices_;
  bool is_final_scan_result_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TetherAvailabilityOperationOrchestratorTest,
       TestWillReturnGmsCoreNotificationsDisabledDevices) {
  FakeTetherAvailabilityOperation::Initializer*
      fake_tether_availability_operation_initializer =
          new FakeTetherAvailabilityOperation::Initializer();

  FakeTetherAvailabilityOperationOrchestrator orchestrator(
      base::WrapUnique<TetherAvailabilityOperation::Initializer>(
          fake_tether_availability_operation_initializer));

  orchestrator.AddObserver(this);
  TetherHost tether_host =
      TetherHost(multidevice::CreateRemoteDeviceRefForTest());
  orchestrator.start_operation_for_device(tether_host);

  fake_tether_availability_operation_initializer->send_result(
      tether_host,
      ScannedDeviceInfo(tether_host.GetDeviceId(), tether_host.GetName(),
                        /*device_status=*/std::nullopt,
                        /*setup_required=*/false,
                        /*notifications_enabled=*/false));

  EXPECT_TRUE(is_final_scan_result_);
}

}  // namespace ash::tether
