// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "base/auto_reset.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "extensions/browser/extension_function.h"
#include "testing/gmock/include/gmock/gmock.h"

class BluetoothPairingDialogTest : public WebUIBrowserTest {
 public:
  BluetoothPairingDialogTest();
  ~BluetoothPairingDialogTest() override;

  void ShowDialog();

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;

  // In the course of running ShowDialog(),
  // BluetoothPrivateConnectFunction::DoWork() is invoked. In this particular
  // test, ExtensionFunction::Respond() is not immediately called because
  // device::BluetoothDevice::Connect() first expects its callbacks to be
  // completed. BluetoothPrivateConnectFunction has no way of invoking those
  // callbacks before ShowDialog() finishes. Until it does, the simplest thing
  // to do is set
  // ExtensionFunction::ignore_all_did_respond_for_testing_do_not_use to true
  // for the duration of the test.
  base::AutoReset<bool> ignore_did_respond;
};

BluetoothPairingDialogTest::BluetoothPairingDialogTest()
    : ignore_did_respond(
          &ExtensionFunction::ignore_all_did_respond_for_testing_do_not_use,
          true) {}

BluetoothPairingDialogTest::~BluetoothPairingDialogTest() {}

void BluetoothPairingDialogTest::ShowDialog() {
  mock_adapter_ = new testing::NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillRepeatedly(testing::Return(true));

  const bool kNotPaired = false;
  const bool kNotConnected = false;
  mock_device_ =
      std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
          nullptr, 0, "Bluetooth 2.0 Mouse", "28:CF:DA:00:00:00", kNotPaired,
          kNotConnected);

  EXPECT_CALL(*mock_adapter_, GetDevice(testing::_))
      .WillRepeatedly(testing::Return(mock_device_.get()));

  chromeos::SystemWebDialogDelegate* dialog =
      chromeos::BluetoothPairingDialog::ShowDialog(
          mock_device_->GetAddress(), mock_device_->GetNameForDisplay(),
          mock_device_->IsPaired(), mock_device_->IsConnected());

  content::WebUI* webui = dialog->GetWebUIForTest();
  content::WebContents* webui_webcontents = webui->GetWebContents();
  content::WaitForLoadStop(webui_webcontents);
  SetWebUIInstance(webui);
}
