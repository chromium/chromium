// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/private/cpp/data_collector.h"

#include "base/functional/callback_forward.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace ash::cros_healthd::internal {
namespace {

constexpr char kFakeTouchpadLibraryName[] = "FakeTouchpadLibraryName";

class FakeDataCollectorDelegate : public DataCollector::Delegate {
 public:
  FakeDataCollectorDelegate() = default;
  ~FakeDataCollectorDelegate() override = default;

  std::string GetTouchpadLibraryName() override {
    return kFakeTouchpadLibraryName;
  }

  bool IsPrivacyScreenSupported() override { return privacy_screen_supported_; }

  bool IsPrivacyScreenManaged() override { return privacy_screen_managed_; }

  void SetPrivacyScreenState(bool state) override {}  // Do nothing.

  void SetPrivacyScreenAttributes(bool supported,
                                  bool managed,
                                  [[maybe_unused]] bool enabled) {
    privacy_screen_supported_ = supported;
    privacy_screen_managed_ = managed;
    // Parameter |enabled| is a no-op situation: whether privacy screen is
    // enabled is not taken care of in the implementation, but remains tested in
    // unittests.
  }

 private:
  bool privacy_screen_supported_ = false;
  bool privacy_screen_managed_ = false;
};

class DataCollectorTest : public testing::Test {
 protected:
  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();
    remote_.Bind(data_collector_.BindNewPipeAndPassRemote());
  }

  void TearDown() override { ui::DeviceDataManager::DeleteInstance(); }

  // The test environment.
  content::BrowserTaskEnvironment env_;
  // The mojo remote to the data collector.
  mojo::Remote<mojom::ChromiumDataCollector> remote_;
  // The fake delegate for DataCollector.
  FakeDataCollectorDelegate delegate_;
  // The DataCollector.
  DataCollector data_collector_{&delegate_};
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

  base::test::TestFuture<std::vector<mojom::TouchscreenDevicePtr>> future;
  remote_->GetTouchscreenDevices(future.GetCallback());
  std::vector<mojom::TouchscreenDevicePtr> expected;
  expected.push_back(mojom::TouchscreenDevice::New(
      mojom::InputDevice::New("DeviceName",
                              mojom::InputDevice::ConnectionType::kBluetooth,
                              "phys", true, "sys_path"),
      42, true, true));
  EXPECT_EQ(future.Take(), expected);
}

TEST_F(DataCollectorTest, GetTouchpadLibraryName) {
  base::test::TestFuture<const std::string&> future;
  remote_->GetTouchpadLibraryName(future.GetCallback());
  EXPECT_EQ(future.Get(), kFakeTouchpadLibraryName);
}

// Test that privacy screen set request will be rejected when privacy screen is
// unsupported.
TEST_F(DataCollectorTest, RejectPrivacyScreenSetRequestOnUnsupported) {
  base::test::TestFuture<bool> future;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/false, /*managed=*/false,
                                       /*enabled=*/false);
  remote_->SetPrivacyScreenState(true, future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Test that privacy screen set request will be rejected when privacy screen is
// in managed mode.
TEST_F(DataCollectorTest, RejectPrivacyScreenSetRequestOnManagedMode) {
  base::test::TestFuture<bool> future;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true, /*managed=*/true,
                                       /*enabled=*/false);
  remote_->SetPrivacyScreenState(true, future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Test that privacy screen set request will be accepted when privacy screen is
// on and is to be turned on.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOnToOn) {
  base::test::TestFuture<bool> future;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true, /*managed=*/false,
                                       /*enabled=*/true);
  remote_->SetPrivacyScreenState(true, future.GetCallback());
  EXPECT_TRUE(future.Get());
}

// Test that privacy screen set request will be accepted when privacy screen is
// on and is to be turned off.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOnToOff) {
  base::test::TestFuture<bool> future;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true,
                                       /*managed=*/false, /*enabled=*/true);
  remote_->SetPrivacyScreenState(false, future.GetCallback());
  EXPECT_TRUE(future.Get());
}

// Test that privacy screen set request will be accepted when privacy screen is
// off and is to be turned on.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOffToOn) {
  base::test::TestFuture<bool> future;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true,
                                       /*managed=*/false, /*enabled=*/false);
  remote_->SetPrivacyScreenState(true, future.GetCallback());
  EXPECT_TRUE(future.Get());
}

// Test that privacy screen set request will be accepted when privacy screen is
// off and is to be turned off.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOffToff) {
  base::test::TestFuture<bool> future;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true,
                                       /*managed=*/false, /*enabled=*/false);
  remote_->SetPrivacyScreenState(false, future.GetCallback());
  EXPECT_TRUE(future.Get());
}

// Test that setting audio output mute always fails.
TEST_F(DataCollectorTest, SetAudioOutputMuteAlwaysFail) {
  base::test::TestFuture<bool> future;
  remote_->DEPRECATED_SetAudioOutputMute(/*mute_on=*/true,
                                         future.GetCallback());
  EXPECT_FALSE(future.Get());

  remote_->DEPRECATED_SetAudioOutputMute(/*mute_on=*/false,
                                         future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace
}  // namespace ash::cros_healthd::internal
