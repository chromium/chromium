// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/data_collector.h"

#include "base/notreached.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {
namespace {

constexpr char kFakeTouchpadLibraryName[] = "FakeTouchpadLibraryName";

class FakeDataCollectorDelegate : public DataCollector::Delegate {
 public:
  FakeDataCollectorDelegate() = default;
  ~FakeDataCollectorDelegate() override = default;

  std::string GetTouchpadLibraryName() override {
    return kFakeTouchpadLibraryName;
  }
};

class DataCollectorTest : public testing::Test {
 protected:
  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();
    DataCollector::InitializeWithDelegateForTesting(&delegate_);
    DataCollector::Get()->BindReceiver(remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    remote_.reset();
    DataCollector::Shutdown();
    ui::DeviceDataManager::DeleteInstance();
  }

  // The test environment.
  content::BrowserTaskEnvironment env_;
  // The mojo remote to the data collector.
  mojo::Remote<mojom::ChromiumDataCollector> remote_;
  // The fake delegate for DataCollector.
  FakeDataCollectorDelegate delegate_;
};

TEST_F(DataCollectorTest, GetTouchscreenDevices) {
  ui::TouchscreenDevice touchscreen_device;
  touchscreen_device.name = "DeviceName";
  touchscreen_device.type = ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH;
  touchscreen_device.phys = "phys";
  touchscreen_device.enabled = true;
  touchscreen_device.sys_path = base::FilePath{"sys_path"};
  touchscreen_device.touch_points = 42;
  touchscreen_device.has_stylus = true;
  touchscreen_device.has_stylus_garage_switch = true;
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({touchscreen_device});

  base::RunLoop run_loop;
  remote_->GetTouchscreenDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::TouchscreenDevicePtr> devices) {
        std::vector<mojom::TouchscreenDevicePtr> expected;
        expected.push_back(mojom::TouchscreenDevice::New(
            mojom::InputDevice::New(
                "DeviceName", mojom::InputDevice::ConnectionType::kBluetooth,
                "phys", true, "sys_path"),
            42, true, true));
        EXPECT_EQ(devices, expected);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DataCollectorTest, GetTouchpadLibraryName) {
  base::RunLoop run_loop;
  remote_->GetTouchpadLibraryName(
      base::BindLambdaForTesting([&](const std::string& library_name) {
        EXPECT_EQ(library_name, kFakeTouchpadLibraryName);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
