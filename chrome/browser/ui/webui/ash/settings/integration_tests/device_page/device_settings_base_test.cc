// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";

}  // namespace

FakeDeviceManager::FakeDeviceManager() = default;
FakeDeviceManager::~FakeDeviceManager() = default;

void FakeDeviceManager::AddFakeInternalKeyboard() {
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
  // Calling RunUntilIdle() here is necessary before setting the keyboard
  // devices to prevent the callback from evdev thread to overwrite whatever
  // we sethere below. See
  // `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  std::map<std::string, std::string> sysfs_properties;
  std::map<std::string, std::string> sysfs_attributes;
  sysfs_properties[kKbdTopRowPropertyName] = kKbdTopRowLayout1Tag;
  fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                           /*subsystem=*/"input", /*devnode=*/std::nullopt,
                           /*devtype=*/std::nullopt,
                           std::move(sysfs_attributes),
                           std::move(sysfs_properties));
}

DeviceSettingsBaseTest::DeviceSettingsBaseTest() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  webcontents_id_ = kOsSettingsWebContentsId;

  feature_list_.InitWithFeatures({features::kInputDeviceSettingsSplit,
                                  features::kAltClickAndSixPackCustomization,
                                  features::kPeripheralCustomization},
                                 {});
}

DeviceSettingsBaseTest::~DeviceSettingsBaseTest() = default;

// `element_id` is the identifier for the top-level Settings window.
// `subpage` contains the page that the Settings app should be launched to.
ui::test::InteractiveTestApi::MultiStep
DeviceSettingsBaseTest::LaunchSettingsApp(
    const ui::ElementIdentifier& element_id,
    const std::string& subpage) {
  return Steps(
      Log(std::format("Open OS Settings to {0}", subpage)),
      InstrumentNextTab(element_id, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), subpage);
      }),
      WaitForShow(element_id),
      Log(std::format("Waiting for OS Settings {0} page to load", subpage)),

      Log("Waiting for OS settings audio settings page to load"),
      WaitForWebContentsReady(element_id, chrome::GetOSSettingsUrl(subpage)));
}

// Enters lower-case text into the focused html input element.
ui::test::InteractiveTestApi::StepBuilder
DeviceSettingsBaseTest::EnterLowerCaseText(const std::string& text) {
  return Do([&]() {
    for (char c : text) {
      ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
          .PressKey(static_cast<ui::KeyboardCode>(ui::VKEY_A + (c - 'a')),
                    ui::EF_NONE, kDeviceId1);
    }
  });
}

ui::test::InteractiveTestApi::StepBuilder
DeviceSettingsBaseTest::SendKeyPressEvent(ui::KeyboardCode key, int modifier) {
  return Do([key, modifier]() {
    ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
        .PressKey(key, modifier, kDeviceId1);
  });
}

void DeviceSettingsBaseTest::SetUpOnMainThread() {
  InteractiveAshTest::SetUpOnMainThread();

  // Set up context for element tracking for InteractiveBrowserTest.
  SetupContextWidget();

  // Ensure the OS Settings system web app (SWA) is installed.
  InstallSystemApps();

  fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
}

ui::test::InteractiveTestApi::StepBuilder
DeviceSettingsBaseTest::SetupInternalKeyboard() {
  return Do([&]() { fake_keyboard_manager_->AddFakeInternalKeyboard(); });
}

void DeviceSettingsBaseTest::SetMouseDevices(
    const std::vector<ui::InputDevice>& mice) {
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetMouseDevices(mice);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
}

void DeviceSettingsBaseTest::SetTouchpadDevices(
    const std::vector<ui::TouchpadDevice>& touchpads) {
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(touchpads);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
}

void DeviceSettingsBaseTest::SetPointingStickDevices(
    const std::vector<ui::InputDevice>& pointing_sticks) {
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(pointing_sticks);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
}

}  // namespace ash
