// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::NiceMock;

using TargetDeviceConnectionBroker =
    ash::quick_start::TargetDeviceConnectionBroker;
using TargetDeviceConnectionBrokerImpl =
    ash::quick_start::TargetDeviceConnectionBrokerImpl;

}  // namespace

class TargetDeviceConnectionBrokerImplTest : public testing::Test {
 public:
  TargetDeviceConnectionBrokerImplTest() = default;
  TargetDeviceConnectionBrokerImplTest(TargetDeviceConnectionBrokerImplTest&) =
      delete;
  TargetDeviceConnectionBrokerImplTest& operator=(
      TargetDeviceConnectionBrokerImplTest&) = delete;
  ~TargetDeviceConnectionBrokerImplTest() override = default;

  void SetUp() override {
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(Invoke(
            this, &TargetDeviceConnectionBrokerImplTest::IsBluetoothPresent));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    CreateConnectionBroker();

    // Allow the Bluetooth adapter to be fetched.
    base::RunLoop().RunUntilIdle();
  }

  void CreateConnectionBroker() {
    connection_broker_ =
        ash::quick_start::TargetDeviceConnectionBrokerFactory::Create();
  }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }

  void SetBluetoothIsPresent(bool present) { is_bluetooth_present_ = present; }

 protected:
  bool is_bluetooth_present_ = true;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(TargetDeviceConnectionBrokerImplTest, GetFeatureSupportStatus) {
  SetBluetoothIsPresent(false);
  EXPECT_EQ(
      TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kNotSupported,
      connection_broker_->GetFeatureSupportStatus());

  SetBluetoothIsPresent(true);
  EXPECT_EQ(TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kSupported,
            connection_broker_->GetFeatureSupportStatus());
}
