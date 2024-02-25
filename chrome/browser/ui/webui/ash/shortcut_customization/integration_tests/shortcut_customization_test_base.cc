// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";

}  // namespace

ShortcutCustomizationInteractiveUiTestBase::
    ShortcutCustomizationInteractiveUiTestBase() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
  webcontents_id_ = kShortcutAppWebContentsId;

  feature_list_.InitWithFeatures({::features::kShortcutCustomization}, {});
}

ShortcutCustomizationInteractiveUiTestBase::
    ~ShortcutCustomizationInteractiveUiTestBase() = default;

void ShortcutCustomizationInteractiveUiTestBase::SetUpOnMainThread() {
  InteractiveAshTest::SetUpOnMainThread();

  // Set up context for element tracking for InteractiveBrowserTest.
  SetupContextWidget();

  // Ensure the Shortcut Customization system web app (SWA) is installed.
  InstallSystemApps();
}

ui::test::InteractiveTestApi::MultiStep
ShortcutCustomizationInteractiveUiTestBase::LaunchShortcutCustomizationApp() {
  return Steps(
      Log("Opening Shortcut Customization app"),
      InstrumentNextTab(webcontents_id_, AnyBrowser()), Do([&]() {
        CreateBrowserWindow(GURL(kChromeUIShortcutCustomizationAppURL));
      }),
      WaitForShow(webcontents_id_),
      Log("Waiting for Shortcut Customization app to load"),
      WaitForWebContentsReady(webcontents_id_,
                              GURL(kChromeUIShortcutCustomizationAppURL)));
}

// Ensure focusing web contents doesn't accidentally block accelerator
// processing. When adding new accelerators, this method is called to
// prevent the system from processing Ash accelerators.
ui::InteractionSequence::StepBuilder
ShortcutCustomizationInteractiveUiTestBase::EnsureAcceleratorsAreProcessed() {
  CHECK(webcontents_id_);
  return ExecuteJs(webcontents_id_,
                   "() => "
                   "document.querySelector('shortcut-customization-app')."
                   "shortcutProvider.preventProcessingAccelerators(false)");
}

ui::test::InteractiveTestApi::MultiStep
ShortcutCustomizationInteractiveUiTestBase::SendShortcutAccelerator(
    ui::Accelerator accel) {
  CHECK(webcontents_id_);
  return Steps(SendAccelerator(webcontents_id_, accel), FlushEvents());
}

ui::test::InteractiveTestApi::MultiStep
ShortcutCustomizationInteractiveUiTestBase::AddKeyboard(bool is_external) {
  return Steps(
      Log(std::format("Adding {0} keyboard",
                      is_external ? "external" : "internal")),
      Do([is_external, this]() {
        int id = kDeviceId1++;
        const auto id_string = base::NumberToString(id);
        ui::KeyboardDevice fake_keyboard(
            /*id=*/id, /*type=*/
            is_external ? ui::InputDeviceType::INPUT_DEVICE_USB
                        : ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
            /*name=*/"Keyboard" + id_string);
        fake_keyboard.sys_path = base::FilePath("path" + id_string);

        fake_keyboard_devices_.push_back(fake_keyboard);
        ui::KeyboardCapability::KeyboardInfo keyboard_info;
        keyboard_info.device_type =
            is_external
                ? ui::KeyboardCapability::DeviceType::
                      kDeviceExternalGenericKeyboard
                : ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
        keyboard_info.top_row_layout = ui::KeyboardCapability::
            KeyboardTopRowLayout::kKbdTopRowLayoutDefault;

        Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
            fake_keyboard, std::move(keyboard_info));
        // Calling RunUntilIdle() here is necessary before setting the keyboard
        // devices to prevent the callback from evdev thread to overwrite
        // whatever we sethere below. See
        // `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
        base::RunLoop().RunUntilIdle();
        ui::DeviceDataManagerTestApi().SetKeyboardDevices(
            fake_keyboard_devices_);
        ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

        std::map<std::string, std::string> sysfs_properties;
        std::map<std::string, std::string> sysfs_attributes;
        sysfs_properties[kKbdTopRowPropertyName] = kKbdTopRowLayout1Tag;
        fake_udev_.AddFakeDevice(
            fake_keyboard.name, fake_keyboard.sys_path.value(),
            /*subsystem=*/"input", /*devnode=*/std::nullopt,
            /*devtype=*/std::nullopt, std::move(sysfs_attributes),
            std::move(sysfs_properties));
      }),
      FlushEvents());
}

ui::test::InteractiveTestApi::MultiStep
ShortcutCustomizationInteractiveUiTestBase::
    WaitForShortcutToContainNumAcceleartors(const DeepQuery& query,
                                            const int expected) {
  return Steps(
      Log(std::format("Expecting shortcut to contain {0} accelerators",
                      expected)),
      CheckJsResultAt(webcontents_id_, query,
                      "e => e.querySelectorAll('accelerator-view').length",
                      expected));
}

}  // namespace ash
