// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chrome/test/base/ash/interactive/webui/interactive_uitest_elements.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/floss_features.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

const char kNewDeviceNickname[] = "awesomeName";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class RenameBluetoothDeviceInteractiveUiTest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  CheckBluetoothDeviceName(const std::string name,
                           const std::string device_address,
                           const std::string device_path) {
    return Steps(
        WaitForElementDoesNotExist(
            kOSSettingsId, ash::settings::bluetooth::BluetoothRenameDialog()),
        WaitForElementTextContains(kOSSettingsId,
                                   settings::bluetooth::BluetoothDeviceName(),
                                   /*text=*/kNewDeviceNickname),
        Do([name, device_address, device_path]() {
          auto* prefs = g_browser_process->local_state();
          DCHECK(prefs);

          base::test::TestFuture<scoped_refptr<device::BluetoothAdapter>>
              adapter;
          device::BluetoothAdapterFactory::Get()->GetAdapter(
              adapter.GetCallback());

          bool device_found = false;

          for (const auto& device : adapter.Get()->GetDevices()) {
            if (device->GetAddress() != device_address) {
              continue;
            }
            const std::string* nickname =
                prefs
                    ->GetDict(floss::features::IsFlossEnabled()
                                  ? ash::bluetooth_config::DeviceNameManager::
                                        kDeviceIdToNicknameMapPrefName
                                  : ash::bluetooth_config::DeviceNameManager::
                                        kDeviceIdToNicknameMapPrefNameLegacy)
                    .FindString(device_path);

            EXPECT_TRUE(nickname && *nickname == name);
            device_found = true;
            break;
          }

          DCHECK(device_found);
        }));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  PerformDeviceRenameSteps(const std::string& device_name) {
    return Steps(

        NavigateToBluetoothDeviceDetailsPage(kOSSettingsId, device_name),
        WaitForElementExists(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothChangeDeviceNameButton()),

        Log("Updating Bluetooth device name"),

        ClickElement(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothChangeDeviceNameButton()),

        WaitForElementExists(kOSSettingsId,
                             ash::settings::bluetooth::BluetoothRenameDialog()),
        ClickElement(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothRenameDialogInputField()),
        ClearInputFieldValue(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothRenameDialogInputField()),
        ClickElement(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothRenameDialogInputField()),
        SendTextAsKeyEvents(kOSSettingsId, kNewDeviceNickname),
        ClickElement(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothRenameDialogDoneButton()));
  }

  base::test::ScopedFeatureList feature_list_;
};

class BluezRenameBluetoothDeviceInteractiveUiTest
    : public RenameBluetoothDeviceInteractiveUiTest {
 public:
  BluezRenameBluetoothDeviceInteractiveUiTest() {
    // Use the legacy BlueZ bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }
};

class FlossRenameBluetoothDeviceInteractiveUiTest
    : public RenameBluetoothDeviceInteractiveUiTest {
 public:
  FlossRenameBluetoothDeviceInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{floss::features::kFlossEnabled},
        /*disabled_features=*/{
            floss::features::kFlossIsAvailabilityCheckNeeded});
  }
};

IN_PROC_BROWSER_TEST_F(BluezRenameBluetoothDeviceInteractiveUiTest,
                       RenameBluetoothDevice) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(
      context,

      Log("Navigating to the Bluetooth details page"),

      PerformDeviceRenameSteps(
          bluez::FakeBluetoothDeviceClient::kPairedDeviceName),

      Log("Checking Bluetooth name is the expected value"),

      CheckBluetoothDeviceName(
          kNewDeviceNickname,
          bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(FlossRenameBluetoothDeviceInteractiveUiTest,
                       RenameBluetoothDevice) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(
      context,

      Log("Navigating to the Bluetooth details page"),

      PerformDeviceRenameSteps(floss::FakeFlossAdapterClient::kBondedAddress2),

      Log("Checking Bluetooth name is the expected value"),

      CheckBluetoothDeviceName(kNewDeviceNickname,
                               floss::FakeFlossAdapterClient::kBondedAddress2,
                               floss::FakeFlossAdapterClient::kBondedAddress2),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
