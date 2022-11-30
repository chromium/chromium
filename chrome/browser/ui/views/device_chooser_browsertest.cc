// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/fake_bluetooth_chooser_controller.h"
#include "components/permissions/fake_usb_chooser_controller.h"
#include "content/public/test/browser_test.h"

namespace {

void ShowChooserBubble(
    Browser* browser,
    std::unique_ptr<permissions::ChooserController> controller) {
  auto* contents = browser->tab_strip_model()->GetActiveWebContents();
  chrome::ShowDeviceChooserDialog(contents->GetPrimaryMainFrame(),
                                  std::move(controller));
}

void ShowChooserModal(
    Browser* browser,
    std::unique_ptr<permissions::ChooserController> controller) {
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  constrained_window::ShowWebModalDialogViews(
      new ChooserDialogView(std::move(controller)), web_contents);
}

void ShowChooser(const std::string& test_name,
                 Browser* browser,
                 std::unique_ptr<permissions::ChooserController> controller) {
  if (base::EndsWith(test_name, "Modal", base::CompareCase::SENSITIVE))
    ShowChooserModal(browser, std::move(controller));
  else
    ShowChooserBubble(browser, std::move(controller));
}

}  // namespace

// Invokes a dialog allowing the user to select a USB device for a web page or
// extension.
class UsbChooserBrowserTest : public DialogBrowserTest {
 public:
  UsbChooserBrowserTest() {}

  UsbChooserBrowserTest(const UsbChooserBrowserTest&) = delete;
  UsbChooserBrowserTest& operator=(const UsbChooserBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowChooser(name, browser(),
                std::make_unique<FakeUsbChooserController>(device_count_));
  }

 protected:
  // Number of devices to show in the chooser.
  int device_count_ = 0;
};

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_NoDevicesBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_NoDevicesModal) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_WithDevicesBubble) {
  device_count_ = 5;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_WithDevicesModal) {
  device_count_ = 5;
  ShowAndVerifyUi();
}

// Invokes a dialog allowing the user to select a Bluetooth device for a web
// page or extension.
class BluetoothChooserBrowserTest : public DialogBrowserTest {
 public:
  BluetoothChooserBrowserTest()
      : status_(permissions::FakeBluetoothChooserController::BluetoothStatus::
                    UNAVAILABLE) {}

  BluetoothChooserBrowserTest(const BluetoothChooserBrowserTest&) = delete;
  BluetoothChooserBrowserTest& operator=(const BluetoothChooserBrowserTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto controller =
        std::make_unique<permissions::FakeBluetoothChooserController>(
            std::move(devices_));
    auto* controller_unowned = controller.get();
    ShowChooser(name, browser(), std::move(controller));
    controller_unowned->SetBluetoothStatus(status_);
  }

  void set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus status) {
    status_ = status;
  }

  void AddDeviceForAllStrengths() {
    devices_.push_back(
        {"Device with Strength 0",
         permissions::FakeBluetoothChooserController::NOT_CONNECTED,
         permissions::FakeBluetoothChooserController::NOT_PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel0});
    devices_.push_back(
        {"Device with Strength 1",
         permissions::FakeBluetoothChooserController::NOT_CONNECTED,
         permissions::FakeBluetoothChooserController::NOT_PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel1});
    devices_.push_back(
        {"Device with Strength 2",
         permissions::FakeBluetoothChooserController::NOT_CONNECTED,
         permissions::FakeBluetoothChooserController::NOT_PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel2});
    devices_.push_back(
        {"Device with Strength 3",
         permissions::FakeBluetoothChooserController::NOT_CONNECTED,
         permissions::FakeBluetoothChooserController::NOT_PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel3});
    devices_.push_back(
        {"Device with Strength 4",
         permissions::FakeBluetoothChooserController::NOT_CONNECTED,
         permissions::FakeBluetoothChooserController::NOT_PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel4});
  }

  void AddConnectedDevice() {
    devices_.push_back(
        {"Connected Device",
         permissions::FakeBluetoothChooserController::CONNECTED,
         permissions::FakeBluetoothChooserController::NOT_PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel4});
  }

  void AddPairedDevice() {
    devices_.push_back(
        {"Paired Device",
         permissions::FakeBluetoothChooserController::NOT_CONNECTED,
         permissions::FakeBluetoothChooserController::PAIRED,
         permissions::FakeBluetoothChooserController::kSignalStrengthLevel4});
  }

 private:
  permissions::FakeBluetoothChooserController::BluetoothStatus status_;
  std::vector<permissions::FakeBluetoothChooserController::FakeDevice> devices_;
};

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest,
                       InvokeUi_UnavailableBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_UnavailableModal) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_NoDevicesBubble) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::IDLE);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_NoDevicesModal) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::IDLE);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ScanningBubble) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ScanningModal) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest,
                       InvokeUi_ScanningWithDevicesBubble) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  AddDeviceForAllStrengths();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest,
                       InvokeUi_ScanningWithDevicesModal) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  AddDeviceForAllStrengths();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ConnectedBubble) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddConnectedDevice();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ConnectedModal) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddConnectedDevice();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_PairedBubble) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddPairedDevice();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_PairedModal) {
  set_status(
      permissions::FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddPairedDevice();
  ShowAndVerifyUi();
}
