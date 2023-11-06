// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bluetooth/bluetooth_dialogs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

#if PAIR_BLUETOOTH_ON_DEMAND()

namespace {

const std::u16string kDeviceIdentifier = u"test-device";

}  // namespace

class BluetoothDeviceCredentialsViewBrowserTest : public DialogBrowserTest {
 public:
  BluetoothDeviceCredentialsViewBrowserTest() = default;
  ~BluetoothDeviceCredentialsViewBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    ShowBluetoothDeviceCredentialsDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), kDeviceIdentifier,
        base::NullCallback());
  }

 private:
  base::WeakPtrFactory<BluetoothDeviceCredentialsViewBrowserTest> weak_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(BluetoothDeviceCredentialsViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

#endif  // PAIR_BLUETOOTH_ON_DEMAND()
