// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_element_identifiers.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/system_toast_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/json/string_escape.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_power_state_observer.h"
#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_util.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chrome/test/base/ash/interactive/webui/interactive_uitest_elements.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_battery_manager_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBluetoothPairingDialogElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

namespace ash {
namespace {

// This JavaScript checks that the code for pairing a Bluetooth device is shown
// in the UI. This JavaScript expects `el` to be the root node on the code entry
// page, and checks that there is a <span> element for each digit in the code
// and for the button indicating the user should afterwards press 'enter'.
constexpr char kCheckPairingCodeJs[] = R"(
  (el) => {
    const code = '%s';
    const elements = el.querySelectorAll('div#code > span');
    if (elements.length != code.length + 1) {
      return false;
    }
    for (let i = 0; i < code.length; i++) {
      if (elements[i].innerText !== code[i]) {
        return false;
      }
    }
    return elements[elements.length - 1].innerText.indexOf('%s') != -1;
  })";

class PairWithUiInteractiveUiTest : public InteractiveAshTest {
 public:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();
  }

  WebContentsInteractionTestUtil::DeepQuery BluetoothPairingDeviceItem() {
    return WebContentsInteractionTestUtil::DeepQuery(
        {"bluetooth-pairing-device-item", "div#deviceName"});
  }

 protected:
  ui::test::internal::InteractiveTestPrivate::MultiStep
  OpenDialogAndClickDevice(const std::string& name) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                        kBluetoothPowerState);

    return Steps(
        ObserveState(kBluetoothPowerState,
                     BluetoothPowerStateObserver::Create()),

        Log("Waiting for Floss clients to be ready (if Floss is enabled)"),

        WaitForFlossClientsReady(),

        Log("Waiting for Bluetooth to be enabled"),

        WaitForState(kBluetoothPowerState, true),

        Log("Opening the Quick Settings bubble"),

        OpenQuickSettings(),

        Log("Navigating to the bluetooth detailed page"),

        NavigateQuickSettingsToBluetoothPage(),

        Log("Waiting for bluetooth toggle button to be shown and enabled"),

        WaitForShow(kBluetoothDetailedViewToggleElementId),
        WaitForViewProperty(kBluetoothDetailedViewToggleElementId, views::View,
                            Enabled, true),

        Log("Waiting for \"pair new device\" button to be be shown"),

        WaitForShow(kBluetoothDetailedViewPairNewDeviceElementId),

        Log("Clicking the \"pair new device\" button"),

        MoveMouseTo(kBluetoothDetailedViewPairNewDeviceElementId), ClickMouse(),

        Log("Waiting for the Quick Settings bubble to close"),

        WaitForHide(kQuickSettingsViewElementId),

        Log("Waiting for the pairing dialog to be visible"),

        InstrumentNonTabWebView(
            kBluetoothPairingDialogElementId,
            BluetoothPairingDialog::kBluetoothPairingDialogElementId),

        Log("Waiting for the device to be visible in the pairing dialog"),

        WaitForElementExists(kBluetoothPairingDialogElementId,
                             webui::bluetooth::PairingDialog()),

        Log(base::StringPrintf("Waiting for Bluetooth device '%s' to be found",
                               name.c_str())),

        WaitForAnyElementTextContains(
            kBluetoothPairingDialogElementId,
            webui::bluetooth::PairingDialogDeviceSelectionPage(),
            BluetoothPairingDeviceItem(),
            /*text=*/name),

        Log("Checking that the device is not paired"),

        CheckBluetoothDevicePairedState(name, /*paired=*/false),

        Log("Clicking the device"),

        ClickAnyElementTextContains(
            kBluetoothPairingDialogElementId,
            webui::bluetooth::PairingDialogDeviceSelectionPage(),
            BluetoothPairingDeviceItem(),
            /*text=*/name));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  CheckDeviceBecomesPaired(const std::string& name) {
    using Observer = views::test::PollingViewObserver<bool, SystemToastView>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kPollingViewState);

    return Steps(
        Log("Waiting for pairing dialog to close"),

        WaitForHide(kBluetoothPairingDialogElementId),

        Log("Waiting for a system toast to become visible and have the "
            "expected text"),

        PollView(
            kPollingViewState, SystemToastView::kSystemToastViewElementId,
            base::BindRepeating(
                [](const std::string name,
                   const SystemToastView* system_toast_view) -> bool {
                  return system_toast_view->GetText() ==
                         l10n_util::GetStringFUTF16(
                             IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
                             base::ASCIIToUTF16(name));
                },
                name),
            base::Milliseconds(50)),

        Log("Checking that the device became paired"),

        CheckBluetoothDevicePairedState(name, /*paired=*/true));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  PerformDeviceForgetSteps(const std::string& device_name) {
    return Steps(
        Log("Navigating to the Bluetooth device details page"),

        NavigateToBluetoothDeviceDetailsPage(kOSSettingsId, device_name),
        WaitForElementExists(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothForgetDeviceButton()),

        Log("Forgetting the Bluetooth device"),

        ClickElement(kOSSettingsId,
                     ash::settings::bluetooth::BluetoothForgetDeviceButton()),
        WaitForElementExists(kOSSettingsId,
                             ash::settings::bluetooth::BluetoothForgetDialog()),
        ClickElement(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothForgetDialogDoneButton()),

        Log("Verifying we are no longer in Bluetooth device details page"),

        WaitForElementDoesNotExist(
            kOSSettingsId, ash::settings::bluetooth::BluetoothForgetDialog()),
        WaitForElementDoesNotExist(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothForgetDeviceButton()),
        WaitForElementExists(kOSSettingsId,
                             ash::settings::bluetooth::BluetoothDeviceList()),

        CheckBluetoothDevicePairedState(device_name, /*paired=*/false));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  PerformCheckDeviceBattery(const std::string& device_name) {
    return Steps(
        Log("Navigating to the Bluetooth device details page"),

        NavigateToBluetoothDeviceDetailsPage(kOSSettingsId, device_name),
        WaitForElementExists(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothForgetDeviceButton()),

        Log("Checking battery percentage"),

        WaitForElementTextContains(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothBatteryPercentage(),
            base::NumberToString(floss::FakeFlossBatteryManagerClient::
                                     kDefaultBatteryPercentage)));
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  PerformOpenPairingDialogSteps() {
    return Steps(
        Log("Navigating to the Bluetooth device details page"),

        NavigateSettingsToBluetoothPage(kOSSettingsId),
        WaitForElementExists(
            kOSSettingsId,
            ash::settings::bluetooth::BluetoothPairNewDeviceButton()),
        ClickElement(kOSSettingsId,
                     ash::settings::bluetooth::BluetoothPairNewDeviceButton()),
        WaitForElementExists(
            kOSSettingsId, ash::settings::bluetooth::BluetoothPairingDialog()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  ui::test::internal::InteractiveTestPrivate::MultiStep
  CheckBluetoothDevicePairedState(const std::string& name, bool paired) {
    return Steps(Do([name, paired]() {
      for (const auto& device : GetBluetoothAdapter()->GetDevices()) {
        EXPECT_TRUE(device->GetName() != name || device->IsPaired() == paired);
      }
    }));
  }

  // TODO(b/353285322): Remove this logic when we are able to rely solely on the
  // adapter powered state for whether we can continue the dependent tests.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForFlossClientsReady() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kFlossClientsReady);
    return Steps(
        ObserveState(
            kFlossClientsReady,
            std::make_unique<ui::test::PollingStateObserver<bool>>(
                base::BindRepeating([]() {
                  return !floss::features::IsFlossEnabled() ||
                         (floss::FlossDBusManager::IsInitialized() &&
                          floss::FlossDBusManager::Get()->AreClientsReady());
                }))),
        WaitForState(kFlossClientsReady, true));
  }
};

class FlossPairWithUiInteractiveUiTest : public PairWithUiInteractiveUiTest {
 public:
  FlossPairWithUiInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{floss::features::kFlossEnabled},
        /*disabled_features=*/{
            floss::features::kFlossIsAvailabilityCheckNeeded});
  }
};

class BluezPairWithUiInteractiveUiTest : public PairWithUiInteractiveUiTest {
 public:
  BluezPairWithUiInteractiveUiTest() {
    // Use the legacy BlueZ bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }
};

IN_PROC_BROWSER_TEST_F(FlossPairWithUiInteractiveUiTest, PairAndForget) {
  RunTestSequence(
      OpenDialogAndClickDevice(floss::FakeFlossAdapterClient::kJustWorksName),

      CheckDeviceBecomesPaired(floss::FakeFlossAdapterClient::kJustWorksName));

  InstallSystemApps();
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(
      context,

      PerformCheckDeviceBattery(floss::FakeFlossAdapterClient::kJustWorksName),

      PerformDeviceForgetSteps(floss::FakeFlossAdapterClient::kJustWorksName),

      Log("Battery percentage should not exist after forgetting devcei"),

      WaitForElementDoesNotExist(
          kOSSettingsId,
          ash::settings::bluetooth::BluetoothBatteryPercentage()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(FlossPairWithUiInteractiveUiTest, PairWithPinCode) {
  RunTestSequence(
      OpenDialogAndClickDevice(
          floss::FakeFlossAdapterClient::kPinCodeDisplayName),

      Log("Waiting to be prompted to enter the code"),

      WaitForElementExists(kBluetoothPairingDialogElementId,
                           webui::bluetooth::PairingDialogEnterCodePage()),

      Log("Checking that the expected code is shown"),

      CheckJsResultAt(
          kBluetoothPairingDialogElementId,
          webui::bluetooth::PairingDialogEnterCodePage(),
          base::StringPrintf(
              kCheckPairingCodeJs, floss::FakeFlossAdapterClient::kPinCode,
              l10n_util::GetStringUTF8(IDS_BLUETOOTH_PAIRING_ENTER_KEY)
                  .c_str())),

      Log("Simulating the pairing process"),

      Do([&]() {
        for (const auto& device : GetBluetoothAdapter()->GetDevices()) {
          if (device->GetName() ==
              floss::FakeFlossAdapterClient::kPinCodeDisplayName) {
            device->SetPinCode(floss::FakeFlossAdapterClient::kPinCode);
            break;
          }
        }
      }),

      CheckDeviceBecomesPaired(
          floss::FakeFlossAdapterClient::kPinCodeDisplayName),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(BluezPairWithUiInteractiveUiTest, SimplePairAndForget) {
  RunTestSequence(OpenDialogAndClickDevice(
                      bluez::FakeBluetoothDeviceClient::kJustWorksName),

                  CheckDeviceBecomesPaired(
                      bluez::FakeBluetoothDeviceClient::kJustWorksName));

  InstallSystemApps();
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(
      context,

      PerformDeviceForgetSteps(floss::FakeFlossAdapterClient::kJustWorksName),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(FlossPairWithUiInteractiveUiTest,
                       OpenPairingDialogFromOsSettings) {
  InstallSystemApps();
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(
      context,

      Log("Opening pairing dialog without pairing to a device"),

      PerformOpenPairingDialogSteps(),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(BluezPairWithUiInteractiveUiTest,
                       OpenPairingDialogFromOsSettings) {
  InstallSystemApps();
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  RunTestSequenceInContext(
      context,

      Log("Opening pairing dialog without pairing to a device"),

      PerformOpenPairingDialogSteps(),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
