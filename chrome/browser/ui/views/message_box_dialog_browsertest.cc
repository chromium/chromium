// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_box_dialog.h"

#include <string>

#include "base/command_line.h"
#include "base/test/test_switches.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"

class MessageBoxDialogUiTest : public UiBrowserTest {
 public:
  // UiBrowserTest:
  void SetUp() override {
    // This test is for manual inspection only. It is not intended to
    // run as an automatic test. This is because the MessageBoxDialog
    // synchronously waits for user actions, either using a run loop or
    // system native dialog (e.g. Win32 MessageBox), and there is no easy way to
    // close it programmatically.
    // You can run this test using
    //   ./browser_tests --gtest_filter=BrowserUiTest.Invoke \
    //     --test-launcher-interactive --ui=MessageBoxDialogUiTest.[TEST_NAME]
    if (!IsInteractiveUi()) {
      GTEST_SKIP();
    }
    UiBrowserTest::SetUp();
  }
  void ShowUi(const std::string& name) override {
    MessageBoxDialog::Show(
        name == "NoParentWindow" ? nullptr
                                 : browser()->window()->GetNativeWindow(),
        u"Title", u"Message", chrome::MESSAGE_BOX_TYPE_WARNING, u"Ok",
        u"Cancel", u"Checkbox Text");
  }

  bool VerifyUi() override {
    views::Widget* message_box_widget =
        MessageBoxDialog::GetLastMessageBoxWidgetForTesting();

    // The message box might be a Cocoa NSAlert or Win32 MessageBox
    // that does not have a views Widget.
    if (!message_box_widget) {
      return true;
    }

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(message_box_widget, test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    views::Widget* message_box_widget =
        MessageBoxDialog::GetLastMessageBoxWidgetForTesting();
    if (message_box_widget) {
      views::test::WidgetDestroyedWaiter(message_box_widget).Wait();
    } else {
      ui_test_utils::WaitForBrowserToClose();
    }
  }
};

// Shows a message box that uses the browser window as its parent.
IN_PROC_BROWSER_TEST_F(MessageBoxDialogUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Shows a message box without a parent window.
IN_PROC_BROWSER_TEST_F(MessageBoxDialogUiTest, InvokeUi_NoParentWindow) {
  ShowAndVerifyUi();
}
