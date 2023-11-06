// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr int kDeviceId1 = 5;
constexpr char kPerDeviceKeyboardSubpagePath[] = "per-device-keyboard";

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeInternalKeyboard() {
    ui::KeyboardDevice fake_keyboard(
        /*id=*/kDeviceId1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
        /*name=*/"Keyboard1");
    fake_keyboard.sys_path = base::FilePath("path1");

    fake_keyboard_devices_.push_back(fake_keyboard);
    ui::KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type =
        ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
    keyboard_info.top_row_layout =
        ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault;

    Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
        fake_keyboard, std::move(keyboard_info));

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = kKbdTopRowLayout1Tag;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                             /*devtype=*/absl::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};

class DeviceSettingsInteractiveUiTest : public InteractiveAshTest {
 public:
  DeviceSettingsInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(features::kInputDeviceSettingsSplit);
  }

  auto AddFakeInternalKeyboard() {
    return Do([&]() { fake_keyboard_manager_->AddFakeInternalKeyboard(); });
  }

  // Query to pierce through Shadow DOM to find the keyboard.
  static DeepQuery kKeyboardNameQuery() {
    return {
        "os-settings-ui",
        "os-settings-main",
        "main-page-container",
        "settings-device-page",
        "settings-per-device-keyboard",
        "settings-per-device-keyboard-subsection",
        "h2#keyboardName",
    };
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, AddNewKeyboard) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Adding a fake internal keyboard"), AddFakeInternalKeyboard(),
      Log("Opening per device keyboard settings subpage"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), kPerDeviceKeyboardSubpagePath);
      }),
      WaitForShow(kOsSettingsWebContentsId),
      Log("Waiting for per device keyboard settings page to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(kPerDeviceKeyboardSubpagePath)),
      Log("Waiting for keyboard to exist"),
      WaitForElementExists(kOsSettingsWebContentsId, kKeyboardNameQuery()),
      CheckJsResultAt(kOsSettingsWebContentsId, kKeyboardNameQuery(),
                      "el => el.innerText", "Built-in Keyboard"),
      Log("Test complete"));
}

}  // namespace
}  // namespace ash
