// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bluetooth_device_pair_confirm_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/bluetooth/bluetooth_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

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

IN_PROC_BROWSER_TEST_P(BluetoothDevicePairConfirmViewBrowserTest,
                       KeyjackingProtectionSafetyWindow) {
  base::test::TestFuture<content::BluetoothDelegate::PairPromptResult> future;

  auto passkey =
      DisplayPasskey() ? std::optional<std::u16string>(kPasskey) : std::nullopt;

  // 1. Instantiate the view and show it using the constrained_window API
  // directly. ShowWebModalDialogViews returns the Widget* pointer directly!
  auto* view = new BluetoothDevicePairConfirmView(
      kDeviceIdentifier, passkey,
      future
          .GetCallback<const content::BluetoothDelegate::PairPromptResult&>());
  views::Widget* dialog_widget = constrained_window::ShowWebModalDialogViews(
      view, browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_NE(dialog_widget, nullptr);

  views::MdTextButton* ok_button = view->GetOkButton();
  ASSERT_NE(ok_button, nullptr);

  // 2. PRESS ENTER IMMEDIATELY (within the 500ms safety window)
  ui::KeyEvent press_enter_soon(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                                ui::EF_NONE, ui::EventTimeForNow());
  views::test::ButtonTestApi(ok_button).NotifyClick(press_enter_soon);

  // VERIFY: The dialog should NOT have closed (input was ignored)
  EXPECT_FALSE(dialog_widget->IsClosed());
  EXPECT_FALSE(future.IsReady());

  // 3. PRESS ENTER AFTER the safety window (Offset by 600ms)
  ui::KeyEvent press_enter_later(
      ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE,
      ui::EventTimeForNow() + base::Milliseconds(600));
  views::test::ButtonTestApi(ok_button).NotifyClick(press_enter_later);

  // VERIFY: The dialog SHOULD be closed now
  EXPECT_TRUE(dialog_widget->IsClosed());
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get().result_code,
            content::BluetoothDelegate::PairPromptStatus::kSuccess);

  // If the test fails and the dialog is somehow not closed, clean it up
  if (!dialog_widget->IsClosed()) {
    dialog_widget->CloseNow();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothDevicePairConfirmViewBrowserTest,
                         testing::Bool());
#endif  // PAIR_BLUETOOTH_ON_DEMAND()
