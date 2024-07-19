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
#include "chrome/browser/ui/webui/ash/bluetooth_pairing_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_power_state_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/webui/interactive_uitest_elements.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

class BluetoothInteractiveUiTest : public InteractiveAshTest {
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
  CheckBluetoothDevicePairedState(const std::string& name, bool paired) {
    return Steps(Do([name, paired]() {
      base::test::TestFuture<scoped_refptr<device::BluetoothAdapter>> adapter;
      device::BluetoothAdapterFactory::Get()->GetAdapter(adapter.GetCallback());
      for (const auto& device : adapter.Get()->GetDevices()) {
        EXPECT_TRUE(device->GetName() != name || device->IsPaired() == paired);
      }
    }));
  }

  base::test::ScopedFeatureList feature_list_;
};

class FlossInteractiveUiTest : public BluetoothInteractiveUiTest {
 public:
  FlossInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{floss::features::kFlossEnabled},
        /*disabled_features=*/{
            floss::features::kFlossIsAvailabilityCheckNeeded});
  }

 protected:
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
                  return floss::FlossDBusManager::IsInitialized() &&
                         floss::FlossDBusManager::Get()->AreClientsReady();
                }))),
        WaitForState(kFlossClientsReady, true));
  }
};

class BluezInteractiveUiTest : public BluetoothInteractiveUiTest {
 public:
  BluezInteractiveUiTest() {
    // Use the legacy BlueZ bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }
};

IN_PROC_BROWSER_TEST_F(FlossInteractiveUiTest, PairDeviceWithQuickSettings) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBluetoothPairingDialogElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                      kBluetoothPowerState);

  RunTestSequence(
      ObserveState(kBluetoothPowerState, BluetoothPowerStateObserver::Create()),

      Log("Waiting for Floss clients to be ready"),

      WaitForFlossClientsReady(),

      Log("Waiting for Bluetooth to be enabled"),

      WaitForState(kBluetoothPowerState, true),

      Log("Opening the Quick Settings bubble"),

      OpenQuickSettings(),

      Log("Navigating to the bluetooth detailed page"),

      NavigateQuickSettingsToBluetoothPage(),

      Log("Waiting for bluetooth toggle button to be shown"),
      WaitForShow(kBluetoothDetailedViewToggleElementId),

      // The bluetooth toggle button may take time to enable because the UI
      // queries the bluetooth adapter state asynchronously.
      Log("Waiting for bluetooth toggle button to enable"),
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
                             floss::FakeFlossAdapterClient::kJustWorksName)),

      WaitForAnyElementTextContains(
          kBluetoothPairingDialogElementId,
          webui::bluetooth::PairingDialogDeviceSelectionPage(),
          BluetoothPairingDeviceItem(),
          /*text=*/floss::FakeFlossAdapterClient::kJustWorksName),

      Log("Checking that the device is not paired"),

      CheckBluetoothDevicePairedState(
          floss::FakeFlossAdapterClient::kJustWorksName, /*paired=*/false),

      Log("Clicking the device"),

      ClickAnyElementTextContains(
          kBluetoothPairingDialogElementId,
          webui::bluetooth::PairingDialogDeviceSelectionPage(),
          BluetoothPairingDeviceItem(),
          /*text=*/floss::FakeFlossAdapterClient::kJustWorksName),

      Log("Waiting for pairing dialog to close"),

      WaitForHide(kBluetoothPairingDialogElementId),

      Log("Waiting for a system toast to become visible and have the expected "
          "text"),

      WaitForShow(SystemToastView::kSystemToastViewElementId),
      CheckViewProperty(
          SystemToastView::kSystemToastViewElementId, &SystemToastView::GetText,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
              base::ASCIIToUTF16(
                  floss::FakeFlossAdapterClient::kJustWorksName))),

      Log("Checking that the device became paired"),

      CheckBluetoothDevicePairedState(
          floss::FakeFlossAdapterClient::kJustWorksName, /*paired=*/true),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(BluezInteractiveUiTest, PairDeviceWithQuickSettings) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBluetoothPairingDialogElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                      kBluetoothPowerState);

  RunTestSequence(
      Log("Waiting for Bluetooth to be enabled"),

      ObserveState(kBluetoothPowerState, BluetoothPowerStateObserver::Create()),
      WaitForState(kBluetoothPowerState, true),

      Log("Opening the Quick Settings bubble"),

      OpenQuickSettings(),

      Log("Navigating to the bluetooth detailed page"),

      NavigateQuickSettingsToBluetoothPage(),

      Log("Waiting for bluetooth toggle button to be shown"),
      WaitForShow(kBluetoothDetailedViewToggleElementId),

      // The bluetooth toggle button may take time to enable because the UI
      // queries the bluetooth adapter state asynchronously.
      Log("Waiting for bluetooth toggle button to enable"),
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
                             bluez::FakeBluetoothDeviceClient::kJustWorksName)),

      WaitForAnyElementTextContains(
          kBluetoothPairingDialogElementId,
          webui::bluetooth::PairingDialogDeviceSelectionPage(),
          BluetoothPairingDeviceItem(),
          bluez::FakeBluetoothDeviceClient::kJustWorksName),

      Log("Checking that the device is not paired"),

      CheckBluetoothDevicePairedState(
          bluez::FakeBluetoothDeviceClient::kJustWorksName, /*paired=*/false),

      Log("Clicking the device"),

      ClickAnyElementTextContains(
          kBluetoothPairingDialogElementId,
          webui::bluetooth::PairingDialogDeviceSelectionPage(),
          BluetoothPairingDeviceItem(),
          bluez::FakeBluetoothDeviceClient::kJustWorksName),

      Log("Waiting for pairing dialog to close"),

      WaitForHide(kBluetoothPairingDialogElementId),

      Log("Waiting for a system toast to become visible and have the expected "
          "text"),

      WaitForShow(SystemToastView::kSystemToastViewElementId),
      CheckViewProperty(
          SystemToastView::kSystemToastViewElementId, &SystemToastView::GetText,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
              base::ASCIIToUTF16(
                  bluez::FakeBluetoothDeviceClient::kJustWorksName))),

      Log("Checking that the device became paired"),

      CheckBluetoothDevicePairedState(
          bluez::FakeBluetoothDeviceClient::kJustWorksName, /*paired=*/true),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
