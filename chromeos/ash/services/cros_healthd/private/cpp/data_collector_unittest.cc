// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/private/cpp/data_collector.h"

#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/test/bind.h"
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

  bool IsOutputForceMuted() override { return audio_force_muted_; }

  void SetOutputMute(bool mute_on) override {}  // Do nothing.

  void SetPrivacyScreenAttributes(bool supported,
                                  bool managed,
                                  [[maybe_unused]] bool enabled) {
    privacy_screen_supported_ = supported;
    privacy_screen_managed_ = managed;
    // Parameter |enabled| is a no-op situation: whether privacy screen is
    // enabled is not taken care of in the implementation, but remains tested in
    // unittests.
  }

  void SetAudioForceMute(bool force_mute) { audio_force_muted_ = force_mute; }

 private:
  bool privacy_screen_supported_ = false;
  bool privacy_screen_managed_ = false;
  bool audio_force_muted_ = false;
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

// Test that privacy screen set request will be rejected when privacy screen is
// unsupported.
TEST_F(DataCollectorTest, RejectPrivacyScreenSetRequestOnUnsupported) {
  base::RunLoop run_loop;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/false, /*managed=*/false,
                                       /*enabled=*/false);
  remote_->SetPrivacyScreenState(
      true, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that privacy screen set request will be rejected when privacy screen is
// in managed mode.
TEST_F(DataCollectorTest, RejectPrivacyScreenSetRequestOnManagedMode) {
  base::RunLoop run_loop;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true, /*managed=*/true,
                                       /*enabled=*/false);
  remote_->SetPrivacyScreenState(
      true, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that privacy screen set request will be accepted when privacy screen is
// on and is to be turned on.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOnToOn) {
  base::RunLoop run_loop;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true, /*managed=*/false,
                                       /*enabled=*/true);
  remote_->SetPrivacyScreenState(
      true, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that privacy screen set request will be accepted when privacy screen is
// on and is to be turned off.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOnToOff) {
  base::RunLoop run_loop;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true,
                                       /*managed=*/false, /*enabled=*/true);
  remote_->SetPrivacyScreenState(
      false, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that privacy screen set request will be accepted when privacy screen is
// off and is to be turned on.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOffToOn) {
  base::RunLoop run_loop;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true,
                                       /*managed=*/false, /*enabled=*/false);
  remote_->SetPrivacyScreenState(
      true, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that privacy screen set request will be accepted when privacy screen is
// off and is to be turned off.
TEST_F(DataCollectorTest, AcceptPrivacyScreenSetRequestFromOffToff) {
  base::RunLoop run_loop;
  delegate_.SetPrivacyScreenAttributes(/*supported=*/true,
                                       /*managed=*/false, /*enabled=*/false);
  remote_->SetPrivacyScreenState(
      false, base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that SetAudioOutputMute will return false when the audio output device
// is force muted but the users want to unmute it.
TEST_F(DataCollectorTest, AudioUnmuteForceMuted) {
  base::RunLoop run_loop;
  delegate_.SetAudioForceMute(/*force_mute=*/true);
  remote_->SetAudioOutputMute(
      /*mute_on=*/false, base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that SetAudioOutputMute will return true when the users want to mute it.
TEST_F(DataCollectorTest, AudioMute) {
  base::RunLoop run_loop;
  delegate_.SetAudioForceMute(/*force_mute=*/true);
  remote_->SetAudioOutputMute(
      /*mute_on=*/true, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that SetAudioOutputMute will return true when the audio output device
// is not force muted.
TEST_F(DataCollectorTest, AudioUnmuteNotForceMuted) {
  base::RunLoop run_loop;
  delegate_.SetAudioForceMute(/*force_mute=*/false);
  remote_->SetAudioOutputMute(
      /*mute_on=*/false, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace ash::cros_healthd::internal
