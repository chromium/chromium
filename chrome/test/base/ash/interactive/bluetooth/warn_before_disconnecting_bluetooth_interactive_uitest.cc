// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_element_identifiers.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_power_state_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chrome/test/base/ash/interactive/webui/interactive_uitest_elements.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {
namespace {

const char kSampleMouseBluetooth[] = "kSampleMouseBluetooth";

// Logitech Vendor ID
const uint16_t kLogitechVID = 0x046d;

// Logitech MX Master 3S Product ID (Bluetooth)
const uint16_t KMousePID = 0xb034;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class WarnBeforeDisconnectingBluetoothInteractiveUiTest
    : public InteractiveAshTest {
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
  PerformOpenWarningDialogSteps() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                        kBluetoothPowerState);

    ui::DeviceDataManagerTestApi().SetMouseDevices(
        {{/*id=*/25, ui::INPUT_DEVICE_BLUETOOTH, kSampleMouseBluetooth,
          /*phys=*/"", base::FilePath(), kLogitechVID, KMousePID,
          /*version=*/0}});

    return Steps(
        ObserveState(kBluetoothPowerState,
                     BluetoothPowerStateObserver::Create()),
        WaitForState(kBluetoothPowerState, true),

        Log("Navigating to the Bluetooth details page"),

        NavigateSettingsToBluetoothPage(kOSSettingsId),
        WaitForElementExists(
            kOSSettingsId, ash::settings::bluetooth::BluetoothSubpageToggle()),
        ClickElement(kOSSettingsId,
                     ash::settings::bluetooth::BluetoothSubpageToggle()),

        Log("Cancel warning dialog keeping Bluetooth on"),

        WaitForShow(kWarnBeforeDisconnectingBluetoothDialogElementId),
        WaitForShow(SystemDialogDelegateView::kCancelButtonIdForTesting),
        MoveMouseTo(SystemDialogDelegateView::kCancelButtonIdForTesting),
        ClickMouse(), WaitForState(kBluetoothPowerState, true),

        Log("Turn Bluetooth off"),

        ClickElement(kOSSettingsId,
                     ash::settings::bluetooth::BluetoothSubpageToggle()),
        WaitForShow(kWarnBeforeDisconnectingBluetoothDialogElementId),
        WaitForShow(SystemDialogDelegateView::kAcceptButtonIdForTesting),
        MoveMouseTo(SystemDialogDelegateView::kAcceptButtonIdForTesting),
        ClickMouse(), WaitForState(kBluetoothPowerState, false));
  }

  base::test::ScopedFeatureList feature_list_;
};

class BluezWarnBeforeDisconnectingBluetoothInteractiveUiTest
    : public WarnBeforeDisconnectingBluetoothInteractiveUiTest {
 public:
  BluezWarnBeforeDisconnectingBluetoothInteractiveUiTest() {
    // Use the legacy BlueZ bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }
};

class FlossWarnBeforeDisconnectingBluetoothInteractiveUiTest
    : public WarnBeforeDisconnectingBluetoothInteractiveUiTest {
 public:
  FlossWarnBeforeDisconnectingBluetoothInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{floss::features::kFlossEnabled},
        /*disabled_features=*/{
            floss::features::kFlossIsAvailabilityCheckNeeded});
  }
};

IN_PROC_BROWSER_TEST_F(BluezWarnBeforeDisconnectingBluetoothInteractiveUiTest,
                       OpenWarningDialog) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(context, PerformOpenWarningDialogSteps(),
                           Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(FlossWarnBeforeDisconnectingBluetoothInteractiveUiTest,
                       OpenWarningDialog) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(context, PerformOpenWarningDialogSteps(),
                           Log("Test complete"));
}

}  // namespace
}  // namespace ash
