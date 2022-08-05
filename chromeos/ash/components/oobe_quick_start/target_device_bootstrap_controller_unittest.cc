// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/target_device_bootstrap_controller.h"
#include <memory>

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
using Observer = TargetDeviceBootstrapController::Observer;
using Status = TargetDeviceBootstrapController::Status;
using Step = TargetDeviceBootstrapController::Step;
using ErrorCode = TargetDeviceBootstrapController::ErrorCode;

class FakeObserver : public Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void OnStatusChanged(const Status& status) override {
    // Step must change.
    ASSERT_NE(status.step, last_status.step);

    last_status = status;
  }

  Status last_status;
};

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
  void TearDown() override {
    bootstrap_controller_->RemoveObserver(fake_observer_.get());
  }

  void CreateBootstrapController() {
    TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);

    bootstrap_controller_ = std::make_unique<TargetDeviceBootstrapController>();
    fake_observer_ = std::make_unique<FakeObserver>();
    bootstrap_controller_->AddObserver(fake_observer_.get());
  }

  FakeTargetDeviceConnectionBroker* connection_broker() {
    EXPECT_EQ(1u, connection_broker_factory_.instances().size());
    return connection_broker_factory_.instances().back();
  }

 protected:
  FakeTargetDeviceConnectionBroker::Factory connection_broker_factory_;
  std::unique_ptr<FakeObserver> fake_observer_;
  std::unique_ptr<TargetDeviceBootstrapController> bootstrap_controller_;
};

TEST_F(TargetDeviceBootstrapControllerTest, StartAdvertising) {
  bootstrap_controller_->StartAdvertising();
  EXPECT_EQ(1u, connection_broker()->num_start_advertising_calls());
  EXPECT_EQ(bootstrap_controller_.get(),
            connection_broker()->connection_lifecycle_listener());

  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);
}

TEST_F(TargetDeviceBootstrapControllerTest, StartAdvertisingFail) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/false);
  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::START_ADVERTISING_FAILED);
}

TEST_F(TargetDeviceBootstrapControllerTest, StopAdvertising) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);

  bootstrap_controller_->StopAdvertising();
  EXPECT_EQ(1u, connection_broker()->num_stop_advertising_calls());

  // Status changes only after the `on_stop_advertising_callback` run.
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);

  connection_broker()->on_stop_advertising_callback().Run();
  EXPECT_EQ(fake_observer_->last_status.step, Step::NONE);
}
