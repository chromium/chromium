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
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/bluetooth_pairing_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_power_state_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/webui/interactive_uitest_elements.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/floss/floss_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

class BluetoothInteractiveUITest : public InteractiveAshTest {
 protected:
  BluetoothInteractiveUITest() {
    // Use the legacy BlueZ bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    fake_bluetooth_device_client_ =
        static_cast<bluez::FakeBluetoothDeviceClient*>(
            bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
    ASSERT_TRUE(fake_bluetooth_device_client_);
  }

  void TearDownOnMainThread() override {
    // Avoid dangling pointers during shutdown.
    fake_bluetooth_device_client_ = nullptr;

    InteractiveAshTest::TearDownOnMainThread();
  }

  WebContentsInteractionTestUtil::DeepQuery BluetoothPairingDeviceItem() {
    return WebContentsInteractionTestUtil::DeepQuery(
        {"bluetooth-pairing-device-item", "div#deviceName"});
  }

 protected:
  ui::test::internal::InteractiveTestPrivate::MultiStep
  CheckBluetoothDevicePairedState(const std::string& path, bool paired) {
    return Steps(Do([this, path, paired]() {
      const bluez::BluetoothDeviceClient::Properties* properties =
          fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(path));
      ASSERT_TRUE(properties);
      ASSERT_EQ(properties->paired.value(), paired);
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<bluez::FakeBluetoothDeviceClient> fake_bluetooth_device_client_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(BluetoothInteractiveUITest,
                       PairDeviceWithQuickSettings) {
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
          bluez::FakeBluetoothDeviceClient::kJustWorksPath, /*paired=*/false),

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
          bluez::FakeBluetoothDeviceClient::kJustWorksPath, /*paired=*/true),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
