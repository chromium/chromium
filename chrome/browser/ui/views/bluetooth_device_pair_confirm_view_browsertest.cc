// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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

constexpr char16_t kDeviceIdentifier[] = u"test-device";
constexpr char16_t kPasskey[] = u"123456";

}  // namespace

class BluetoothDevicePairConfirmViewBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BluetoothDevicePairConfirmViewBrowserTest() = default;
  BluetoothDevicePairConfirmViewBrowserTest(
      const BluetoothDevicePairConfirmViewBrowserTest&) = delete;
  BluetoothDevicePairConfirmViewBrowserTest& operator=(
      const BluetoothDevicePairConfirmViewBrowserTest&) = delete;
  ~BluetoothDevicePairConfirmViewBrowserTest() override = default;

  bool DisplayPasskey() { return GetParam(); }

  void ShowUi(const std::string& name) override {
    auto passkey = DisplayPasskey() ? std::optional<std::u16string>(kPasskey)
                                    : std::nullopt;

    ShowBluetoothDevicePairConfirmDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), kDeviceIdentifier,
        passkey, base::NullCallback());
  }
};

IN_PROC_BROWSER_TEST_P(BluetoothDevicePairConfirmViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothDevicePairConfirmViewBrowserTest,
                         testing::Bool());
#endif  // PAIR_BLUETOOTH_ON_DEMAND()
