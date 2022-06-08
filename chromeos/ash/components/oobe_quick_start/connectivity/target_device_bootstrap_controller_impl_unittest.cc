// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_bootstrap_controller_impl.h"

#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::NiceMock;

using TargetDeviceBootstrapController =
    ash::quick_start::TargetDeviceBootstrapController;
using TargetDeviceBootstrapControllerImpl =
    ash::quick_start::TargetDeviceBootstrapControllerImpl;

}  // namespace

class TargetDeviceBootstrapControllerImplTest : public ::testing::Test {
 public:
  TargetDeviceBootstrapControllerImplTest() = default;
  TargetDeviceBootstrapControllerImplTest(
      TargetDeviceBootstrapControllerImplTest&) = delete;
  TargetDeviceBootstrapControllerImplTest& operator=(
      TargetDeviceBootstrapControllerImplTest&) = delete;
  ~TargetDeviceBootstrapControllerImplTest() override = default;

  void SetUp() override {
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(Invoke(
            this,
            &TargetDeviceBootstrapControllerImplTest::IsBluetoothPresent));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    CreateBootstrapController();

    // Allow the Bluetooth adapter to be fetched.
    base::RunLoop().RunUntilIdle();
  }

  void CreateBootstrapController() {
    bootstrap_controller_ =
        std::make_unique<TargetDeviceBootstrapControllerImpl>();
  }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }

  void SetBluetoothIsPresent(bool present) { is_bluetooth_present_ = present; }

 protected:
  bool is_bluetooth_present_ = true;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  std::unique_ptr<TargetDeviceBootstrapController> bootstrap_controller_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(TargetDeviceBootstrapControllerImplTest, GetFeatureSupportStatus) {
  SetBluetoothIsPresent(false);
  EXPECT_EQ(
      TargetDeviceBootstrapControllerImpl::FeatureSupportStatus::kNotSupported,
      bootstrap_controller_->GetFeatureSupportStatus());

  SetBluetoothIsPresent(true);
  EXPECT_EQ(
      TargetDeviceBootstrapControllerImpl::FeatureSupportStatus::kSupported,
      bootstrap_controller_->GetFeatureSupportStatus());
}
