// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_inhibitor.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/fake_shill_device_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_device_handler_impl.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";

enum class GetInhibitedPropertyResult { kTrue, kFalse, kOperationFailed };

}  // namespace

class CellularInhibitorTest : public testing::Test {
 protected:
  CellularInhibitorTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        helper_(/*use_default_devices_and_services=*/false) {}
  ~CellularInhibitorTest() override = default;

  // testing::Test:
  void SetUp() override {
    helper_.device_test()->ClearDevices();
    cellular_inhibitor_.Init(helper_.network_state_handler(),
                             helper_.network_device_handler());
  }

  void AddCellularDevice() {
    helper_.device_test()->AddDevice(kDefaultCellularDevicePath,
                                     shill::kTypeCellular, "cellular1");
    base::RunLoop().RunUntilIdle();
  }

  bool InhibitCellularScanning() {
    return SetInhibitProperty(/*new_inhibit_value=*/true);
  }

  bool UninhibitCellularScanning() {
    return SetInhibitProperty(/*new_inhibit_value=*/false);
  }

  GetInhibitedPropertyResult GetInhibitedProperty() {
    properties_.reset();
    helper_.network_device_handler()->GetDeviceProperties(
        kDefaultCellularDevicePath,
        base::BindOnce(&CellularInhibitorTest::GetPropertiesCallback,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    if (!properties_)
      return GetInhibitedPropertyResult::kOperationFailed;

    bool inhibited;
    EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
        shill::kInhibitedProperty, &inhibited));
    return inhibited ? GetInhibitedPropertyResult::kTrue
                     : GetInhibitedPropertyResult::kFalse;
  }

 private:
  bool SetInhibitProperty(bool new_inhibit_value) {
    base::RunLoop run_loop;
    on_result_closure_ = run_loop.QuitClosure();

    if (new_inhibit_value) {
      cellular_inhibitor_.InhibitCellularScanning(
          base::BindOnce(&CellularInhibitorTest::OnInhibitOrUninhibitResult,
                         base::Unretained(this)));
    } else {
      cellular_inhibitor_.UninhibitCellularScanning(
          base::BindOnce(&CellularInhibitorTest::OnInhibitOrUninhibitResult,
                         base::Unretained(this)));
    }
    run_loop.Run();

    return success_;
  }

  void OnInhibitOrUninhibitResult(bool success) {
    success_ = success;
    std::move(on_result_closure_).Run();
  }

  void GetPropertiesCallback(const std::string& device_path,
                             base::Optional<base::Value> properties) {
    if (!properties) {
      properties_.reset();
      return;
    }

    properties_ = base::DictionaryValue::From(
        std::make_unique<base::Value>(std::move(*properties)));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_;
  CellularInhibitor cellular_inhibitor_;

  bool success_;
  std::unique_ptr<base::DictionaryValue> properties_;
  base::OnceClosure on_result_closure_;
};

TEST_F(CellularInhibitorTest, Success) {
  AddCellularDevice();

  EXPECT_TRUE(InhibitCellularScanning());
  EXPECT_EQ(GetInhibitedPropertyResult::kTrue, GetInhibitedProperty());

  // Inhibit while already inhibited should succeed.
  EXPECT_TRUE(InhibitCellularScanning());
  EXPECT_EQ(GetInhibitedPropertyResult::kTrue, GetInhibitedProperty());

  EXPECT_TRUE(UninhibitCellularScanning());
  EXPECT_EQ(GetInhibitedPropertyResult::kFalse, GetInhibitedProperty());

  // Uninhibit while already uninhibited should succeed.
  EXPECT_TRUE(UninhibitCellularScanning());
  EXPECT_EQ(GetInhibitedPropertyResult::kFalse, GetInhibitedProperty());
}

TEST_F(CellularInhibitorTest, Failure) {
  // Do not add a Cellular device. This should cause commands below to fail,
  // since the device cannot be inhibited if it does not exist.

  EXPECT_FALSE(InhibitCellularScanning());
  EXPECT_EQ(GetInhibitedPropertyResult::kOperationFailed,
            GetInhibitedProperty());
  EXPECT_FALSE(UninhibitCellularScanning());
  EXPECT_EQ(GetInhibitedPropertyResult::kOperationFailed,
            GetInhibitedProperty());
}

}  // namespace chromeos
