// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/target_device_bootstrap_controller.h"

#include "chromeos/ash/components/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using TargetDeviceBootstrapController =
    ash::quick_start::TargetDeviceBootstrapController;
using TargetDeviceConnectionBroker =
    ash::quick_start::TargetDeviceConnectionBroker;
using FakeTargetDeviceConnectionBroker =
    ash::quick_start::FakeTargetDeviceConnectionBroker;
using TargetDeviceConnectionBrokerFactory =
    ash::quick_start::TargetDeviceConnectionBrokerFactory;

}  // namespace

class TargetDeviceBootstrapControllerTest : public testing::Test {
 public:
  TargetDeviceBootstrapControllerTest() = default;
  TargetDeviceBootstrapControllerTest(TargetDeviceBootstrapControllerTest&) =
      delete;
  TargetDeviceBootstrapControllerTest& operator=(
      TargetDeviceBootstrapControllerTest&) = delete;
  ~TargetDeviceBootstrapControllerTest() override = default;

  void SetUp() override { CreateBootstrapController(); }

  void CreateBootstrapController() {
    TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);

    bootstrap_controller_ = std::make_unique<TargetDeviceBootstrapController>();
  }

  FakeTargetDeviceConnectionBroker* connection_broker() {
    EXPECT_EQ(1u, connection_broker_factory_.instances().size());
    return connection_broker_factory_.instances().back();
  }

 protected:
  FakeTargetDeviceConnectionBroker::Factory connection_broker_factory_;
  std::unique_ptr<TargetDeviceBootstrapController> bootstrap_controller_;
};

TEST_F(TargetDeviceBootstrapControllerTest, StartAdvertising) {
  bootstrap_controller_->StartAdvertising();
  EXPECT_EQ(1u, connection_broker()->num_start_advertising_calls());
  EXPECT_EQ(bootstrap_controller_.get(),
            connection_broker()->connection_lifecycle_listener());
}

TEST_F(TargetDeviceBootstrapControllerTest, StopAdvertising) {
  bootstrap_controller_->StopAdvertising();
  EXPECT_EQ(1u, connection_broker()->num_stop_advertising_calls());
}
